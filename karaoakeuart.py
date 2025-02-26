import serial
import time
import os
import pyaudio
import wave
import threading
import re
from dataclasses import dataclass
from typing import List, Optional
from ctypes import cast, POINTER
from comtypes import CLSCTX_ALL
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume

# Setup Definitions
UART_COM_PORT = "COM6"
BAUD_RATE = 115200
SONG_DIRECTORY = r"C:\Users\mclemoew\OneDrive - Rose-Hulman Institute of Technology\Desktop\ece230karaoke\audio"
LYRIC_DIRECTORY = r"C:\Users\mclemoew\OneDrive - Rose-Hulman Institute of Technology\Desktop\ece230karaoke\lyrics"

# UART to ESP32
ser = serial.Serial(UART_COM_PORT, BAUD_RATE, timeout=1)

# Playback states
current_song_index = None
is_playing = False
is_reset = False
stop_audio = threading.Event()
resume_position = 0
last_resume_position = 0
current_lyric_thread: Optional[threading.Thread] = None
stop_lyrics = threading.Event()
next_lyric_index = 0

# Tracking old commands
old_isPlaying = None
old_songIndex = None
old_isReset = None

# Regex patterns for PSR commands
COMMAND_PATTERN = re.compile(r"P:(\d+) S:(\d+) R:(\d+)")


@dataclass
class LyricLine:
    timestamp: float  # when to play lyric
    text: str  # which lyric to play


def setup_volume_control():  # Initialize Windows volume control
    devices = AudioUtilities.GetSpeakers()
    interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
    return cast(interface, POINTER(IAudioEndpointVolume))


def set_system_volume(volume_control, percentage):  # Sets windows volume to correct value
    try:
        volume_db = volume_control.GetVolumeRange()[0] + (
                volume_control.GetVolumeRange()[1] - volume_control.GetVolumeRange()[0]) * (percentage / 100)
        volume_control.SetMasterVolumeLevel(volume_db, None)
    except Exception as e:
        print(f"Error setting volume: {e}")


def parse_lyric_file(song_index: int) -> List[LyricLine]:  # gathers audio and lyrics for current song
    song_name = songs[song_index].replace('.wav', '')
    lyric_file = os.path.join(LYRIC_DIRECTORY, f"{song_name}.txt")

    lyrics = []
    try:
        with open(lyric_file, 'r', encoding='utf-8') as f:
            for line in f:
                # Expected format: [MM:SS.ms] Lyric text
                if line.startswith('[') and ']' in line:
                    time_str = line[1:line.index(']')]
                    text = line[line.index(']') + 1:].strip()

                    # Parse timestamp
                    minutes, seconds = time_str.split(':')
                    timestamp = float(minutes) * 60 + float(seconds)

                    lyrics.append(LyricLine(timestamp, text))
    except FileNotFoundError:
        print(f"No lyrics file found for {song_name}")
    except Exception as e:
        print(f"Error parsing lyrics file: {e}")

    return sorted(lyrics, key=lambda x: x.timestamp)


def split_lyric_into_chunks(lyric: str, max_line_length: int = 16, max_display_lines: int = 2) -> List[tuple]:
    # splits lyrics into appropriate chunks to display on LCD
    words = lyric.split()
    chunks = []
    current_chunk = []
    current_line = ""
    current_word_count = 0

    for word in words:
        if len(current_line) + len(word) + 1 <= max_line_length:
            current_line += (word + " ")
            current_word_count += 1
        else:
            current_chunk.append(current_line.strip())
            current_line = word + " "
            current_word_count += 1

            if len(current_chunk) == max_display_lines:
                chunks.append(("|".join(current_chunk), current_word_count))
                current_chunk = []
                current_word_count = 0

    if current_line.strip():
        current_chunk.append(current_line.strip())

    if current_chunk:
        chunks.append(("|".join(current_chunk), current_word_count))

    return chunks


def send_lyrics_to_esp32(lyrics: List[LyricLine], wave_position: float):
    # Sends lyrics synced with audio to ESP32
    global is_playing, stop_lyrics

    start_time = time.time()

    # Skip lines behind current song position
    idx = 0
    while idx < len(lyrics) and lyrics[idx].timestamp < wave_position:
        idx += 1

    print(f"Lyric thread: starting from index={idx}, wave_position={wave_position:.2f}")

    while idx < len(lyrics):
        if stop_lyrics.is_set() or not is_playing:
            break

        lyric = lyrics[idx]
        line_time = lyric.timestamp - wave_position
        wait_time = line_time - (time.time() - start_time)

        while wait_time > 0.1 and not stop_lyrics.is_set() and is_playing:
            time.sleep(0.1)
            wait_time -= 0.1

        if stop_lyrics.is_set() or not is_playing:
            break

        chunks = split_lyric_into_chunks(lyric.text, 16, 2)
        for txt, word_count in chunks:
            if stop_lyrics.is_set() or not is_playing:
                break

            ascii_txt = txt.encode('ascii', 'ignore').decode('ascii')
            ascii_txt = ''.join(c for c in ascii_txt if 32 <= ord(c) <= 126)
            if '|' not in ascii_txt:
                ascii_txt += '|'

            ser.write(f"L:{ascii_txt}\n".encode('ascii', 'ignore'))
            ser.flush()

            print(f"Sent lyric: {ascii_txt.replace('|', ' | ')}")

            word_delay = max(0.5, word_count * 0.25)
            elapsed = 0
            while elapsed < word_delay and not stop_lyrics.is_set() and is_playing:
                time.sleep(0.1)
                elapsed += 0.1

        idx += 1

    print("Lyric thread done or paused/reset.")


def play_audio(song_index: Optional[int], start_frame=0):
    # Plays the selected song and synchronizes lyrics
    global is_playing, current_song_index, resume_position, last_resume_position
    global current_lyric_thread, stop_audio, stop_lyrics

    if song_index is None or not isinstance(song_index, int) or song_index < 0 or song_index >= len(songs):
        print(f"Invalid song index: {song_index}")
        return

    song_path = os.path.join(SONG_DIRECTORY, songs[song_index])
    print(f"Playing {song_path} from frame {start_frame}...")

    try:
        # Open audio file and parse lyrics
        wf = wave.open(song_path, 'rb')
        lyrics = parse_lyric_file(song_index)

        # Setup audio
        p = pyaudio.PyAudio()
        chunk_size = 1024
        stream = p.open(
            format=p.get_format_from_width(wf.getsampwidth()),
            channels=wf.getnchannels(),
            rate=wf.getframerate(),
            output=True,
            frames_per_buffer=chunk_size
        )

        # Calculate wave position in seconds
        wave_position = float(start_frame) / wf.getframerate()
        print(f"wave_position = {wave_position:.2f}s (start_frame={start_frame})")

        stop_audio.clear()
        stop_lyrics.clear()

        # Start lyric thread
        current_lyric_thread = threading.Thread(
            target=send_lyrics_to_esp32,
            args=(lyrics, wave_position)
        )
        current_lyric_thread.start()

        wf.setpos(start_frame)

        # Plays audio
        data = wf.readframes(chunk_size)
        while data and is_playing and not stop_audio.is_set():
            stream.write(data)
            last_resume_position = wf.tell()
            data = wf.readframes(chunk_size)

        # Handle end of song playback
        if not data:
            print("Song ended!")
            is_playing = False
            resume_position = 0
            last_resume_position = 0
        else:
            resume_position = last_resume_position
            print(f"Paused at frame={resume_position} (time={resume_position / wf.getframerate():.2f}s)")

        # Cleanup lyrics and playback
        stop_lyrics.set()
        if current_lyric_thread:
            current_lyric_thread.join()

        stream.stop_stream()
        stream.close()
        p.terminate()
        wf.close()

    except Exception as e:
        print(f"Error in play_audio: {e}")
        stop_lyrics.set()
        if current_lyric_thread and current_lyric_thread.is_alive():
            current_lyric_thread.join()


def read_esp32_data():
    # Listens for PSR commands and volume control from ESP32
    global is_playing, current_song_index, stop_audio
    global resume_position, last_resume_position, is_reset, stop_lyrics
    global old_isPlaying, old_songIndex, old_isReset

    # Initialize volume control
    try:
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(
            IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        volume_control = cast(interface, POINTER(IAudioEndpointVolume))
        print("Volume control initialized")
    except Exception as e:
        print(f"Failed to initialize volume control: {e}")
        volume_control = None

    print("Listening for ESP32 playback commands...")

    while True:
        try:
            data = ser.readline().decode('utf-8', errors='ignore').strip()
            if not data:
                continue

            print(f"RAW UART Data from ESP32: {data}")

            # Handle volume commands
            if data.startswith("V:") and volume_control:
                try:
                    volume_percent = int(data[2:])
                    volume_percent = max(0, min(100, volume_percent))
                    min_db, max_db, _ = volume_control.GetVolumeRange()
                    volume_db = min_db + (max_db - min_db) * (volume_percent / 100)
                    volume_control.SetMasterVolumeLevel(volume_db, None)
                    print(f"Volume set to {volume_percent}%")
                except ValueError:
                    print("Invalid volume value")
                continue

            # Handle PSR commands
            match = COMMAND_PATTERN.match(data)
            if match:
                new_is_playing = int(match.group(1))
                new_song_index = int(match.group(2))
                new_is_reset = int(match.group(3))

                print(f"Extracted -> isPlaying={new_is_playing}, "
                      f"songIndex={new_song_index}, isReset={new_is_reset}")

                # Avoid duplicates
                if (new_is_playing == old_isPlaying and
                        new_song_index == old_songIndex and
                        new_is_reset == old_isReset):
                    print("Duplicate command. Ignoring.")
                    continue

                old_isPlaying = new_is_playing
                old_songIndex = new_song_index
                old_isReset = new_is_reset

                # Handle resets
                if new_is_reset:
                    print("Reset triggered!")
                    is_playing = False
                    stop_audio.set()
                    stop_lyrics.set()
                    if current_lyric_thread and current_lyric_thread.is_alive():
                        current_lyric_thread.join(timeout=1.0)
                    resume_position = 0
                    last_resume_position = 0
                    stop_audio.clear()
                    stop_lyrics.clear()
                    print("Reset done. Ready for new playback.")

                # Validate song index
                if new_song_index < 0 or new_song_index >= len(songs):
                    print(f"Invalid new_song_index: {new_song_index}")
                    continue

                # Start or resume playback
                if new_is_playing and not is_playing:
                    print(f"Starting/resuming from frame={last_resume_position}")
                    is_playing = True
                    current_song_index = new_song_index

                    stop_audio.clear()
                    stop_lyrics.clear()

                    audio_thread = threading.Thread(
                        target=play_audio,
                        args=(current_song_index, last_resume_position)
                    )
                    audio_thread.start()

                # Pause playback
                elif not new_is_playing and is_playing:
                    print("Pausing playback")
                    is_playing = False
                    stop_audio.set()
                    stop_lyrics.set()
                    if current_lyric_thread and current_lyric_thread.is_alive():
                        current_lyric_thread.join(timeout=1.0)

        except Exception as e:
            print(f"Error in read_esp32_data: {e}")


# Initializes songs
songs = sorted([f for f in os.listdir(SONG_DIRECTORY) if f.endswith(".wav")],
               key=lambda x: int(x.split("_")[0]))
print(f"Found {len(songs)} songs: {songs}")

# Starts main loop
if __name__ == "__main__":
    read_esp32_data()
