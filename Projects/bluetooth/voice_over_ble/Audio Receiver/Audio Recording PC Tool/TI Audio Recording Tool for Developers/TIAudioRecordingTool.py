import serial
import struct
import threading
import time
import numpy as np
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from scipy.io.wavfile import write
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# -----------------------------
# DEFAULT SETTINGS
# -----------------------------
BAUD_RATE = 3000000   # UART Baudrate, same as in embedded project.
SAMPLE_RATE = 16000   # Audio Sampling Frequency
SAMPLE_BYTES = 2      # uint16 samples
WINDOW_SIZE = 2000    # plotting window size

# -----------------------------
# GLOBALS
# -----------------------------
stop_flag = False
data_buffer = []
ser = None

# -----------------------------
# SERIAL READER
# -----------------------------
def serial_reader(port_name, rec_time):
    global ser, data_buffer, stop_flag

    try:
        ser = serial.Serial(port_name, BAUD_RATE, timeout=0)
    except Exception as e:
        root.after(0, lambda: messagebox.showerror("Error", f"Cannot open serial port:\n{e}"))
        return

    data_buffer = []
    start_time = time.time()

    while (time.time() - start_time < rec_time) and not stop_flag:

        # Read ALL bytes currently in UART FIFO
        bytes_waiting = ser.in_waiting

        if bytes_waiting > 0:
            raw = ser.read(bytes_waiting)

            # drop odd byte
            usable = len(raw) - (len(raw) % 2)
            if usable > 0:
                samples = np.frombuffer(raw[:usable], dtype="<u2")
                data_buffer.extend(samples)

        time.sleep(0.0005)  # allow UART buffer to fill

    ser.close()

    root.after(0, lambda: messagebox.showinfo("Done", "Recording finished."))

    # auto-save
    auto_filename = wav_name_entry.get() or "capture.wav"
    save_wav(auto_filename)

# -----------------------------
# SAVE TO .WAV
# -----------------------------
def save_wav(filename):
    if len(data_buffer) == 0:
        messagebox.showwarning("Warning", "No data recorded yet.")
        return

    arr = np.array(data_buffer, dtype=np.uint16).reshape(-1)
    audio = ((arr.astype(np.uint16) - 2048)*16).astype(np.int16)  # considering a 12 ADC-bit
    if not filename.lower().endswith(".wav"):
        filename += ".wav"

    write(filename, SAMPLE_RATE, audio)

    messagebox.showinfo("Saved", f"WAV saved as:\n{filename}")
    print("Saved:", filename)
    print("Samples Recorded:", len(data_buffer))

# -----------------------------
# PLOTTING
# -----------------------------
def update_plot():
    if len(data_buffer) > 0:
        window = data_buffer[-WINDOW_SIZE:]
        x = np.arange(len(window))
        line.set_data(x, window)
        ax.set_xlim(0, len(window))

    canvas.draw()
    root.after(50, update_plot)

# -----------------------------
# CALLBACKS
# -----------------------------
def start_capture():
    global stop_flag
    stop_flag = False

    port = port_combo.get()
    if not port:
        messagebox.showerror("Error", "Select a Serial Port.")
        return

    try:
        rec_time = float(record_time_entry.get())
    except ValueError:
        messagebox.showerror("Error", "Recording time must be a number.")
        return

    threading.Thread(
        target=serial_reader,
        args=(port, rec_time),
        daemon=True
    ).start()

def stop_capture():
    global stop_flag
    stop_flag = True
    messagebox.showinfo("Stop", "Stopping capture...")

def save_wav_manual():
    filename = wav_name_entry.get()
    if filename.strip() == "":
        filename = "capture.wav"
    save_wav(filename)

# -----------------------------
# GUI SETUP
# -----------------------------
root = tk.Tk()
root.title("Real-Time Audio Capture Tool")
root.geometry("900x750")

# -----------------------------
# LOGO + TITLE
# -----------------------------
try:
    img = Image.open("TexasInstrumentsLogo.png")
    img = img.resize((220, 80))
    logo_img = ImageTk.PhotoImage(img)

    logo_label = tk.Label(root, image=logo_img)
    logo_label.pack(pady=5)
except:
    tk.Label(root, text="[Logo Missing: Place 'logo.png' in folder]").pack()

tk.Label(root, text="Real-Time Audio Acquisition & WAV Recorder",
         font=("Arial", 18, "bold")).pack(pady=5)


# -----------------------------
# CONTROL PANEL
# -----------------------------
settings_frame = tk.Frame(root)
settings_frame.pack(pady=10)

tk.Label(settings_frame, text="Serial Port:").grid(row=0, column=0, padx=5, pady=5)
port_combo = tk.Entry(settings_frame)
port_combo.insert(0, "COM18")
port_combo.grid(row=0, column=1, padx=5, pady=5)

tk.Label(settings_frame, text="Recording Time (s):").grid(row=1, column=0, padx=5, pady=5)
record_time_entry = tk.Entry(settings_frame)
record_time_entry.insert(0, "15")
record_time_entry.grid(row=1, column=1, padx=5, pady=5)

tk.Label(settings_frame, text="WAV File Name:").grid(row=2, column=0, padx=5, pady=5)
wav_name_entry = tk.Entry(settings_frame)
wav_name_entry.insert(0, "capture.wav")
wav_name_entry.grid(row=2, column=1, padx=5, pady=5)


# Buttons
tk.Button(settings_frame, text="Start Capture", width=18, command=start_capture)\
    .grid(row=3, column=0, padx=5, pady=10)

tk.Button(settings_frame, text="Stop Capture", width=18, command=stop_capture)\
    .grid(row=3, column=1, padx=5, pady=10)

tk.Button(settings_frame, text="Save WAV Now", width=18, command=save_wav_manual)\
    .grid(row=4, column=0, columnspan=2, pady=5)


# -----------------------------
# MATPLOTLIB FIGURE
# -----------------------------
fig, ax = plt.subplots(figsize=(8,4))
line, = ax.plot([], [])
ax.set_ylim(0, 4000)
ax.set_xlim(0, WINDOW_SIZE)
ax.set_title("Audio Data")
ax.set_xlabel("Sample Index")
ax.set_ylabel("Amplitude")

canvas = FigureCanvasTkAgg(fig, master=root)
canvas.get_tk_widget().pack(fill="both", expand=True)

root.after(50, update_plot)
root.mainloop()
