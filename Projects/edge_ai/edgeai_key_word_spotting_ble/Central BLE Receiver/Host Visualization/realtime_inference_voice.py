"""
Read binary UART packets (1-byte class + 3 float32 softmax probs + 128 raw bytes)
and display real-time plots. Allows saving current raw data to CSV via GUI button.
"""

import serial
import struct
import threading
import time
import numpy as np
from collections import deque
import plotly.graph_objects as go
from dash import Dash, dcc, html, Input, Output, State
import csv

# ---------- UART CONFIG ----------
SERIAL_PORT = "COM177"           # Change this to your port
BAUD_RATE = 1500000
FRAME_SIZE = 17                 # 1 (uint8 class) + 4 (float32 probabilities)
REFRESH_INTERVAL_MS = 500
BUFFER_SIZE = 200
SAMPLES_TO_SHOW = 10

# ---------- BUFFERS ----------
p0_buf = deque(maxlen=BUFFER_SIZE)
p1_buf = deque(maxlen=BUFFER_SIZE)
p2_buf = deque(maxlen=BUFFER_SIZE)
p3_buf = deque(maxlen=BUFFER_SIZE)
cls_buf = deque(maxlen=BUFFER_SIZE)
index_buf = deque(maxlen=BUFFER_SIZE)
raw_buf = deque(maxlen=BUFFER_SIZE)  # each entry holds one 17 bytes.


# ---------- UART READER THREAD ----------
def uart_reader():
    print(f"Reading UART {SERIAL_PORT} @ {BAUD_RATE} baud...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    buf = bytearray()
    i = 0

    while True:
        try:
            buf += ser.read(FRAME_SIZE - len(buf))
            if len(buf) < FRAME_SIZE:
                continue

            frame = bytes(buf[:FRAME_SIZE])
            buf = buf[FRAME_SIZE:]

            # Parse UART frame
            cls, p0, p1, p2, p3 = struct.unpack('<Bffff', frame[:17])  # Little-endian
            raw_data = np.zeros(FRAME_SIZE)

            raw_buf.append(raw_data)
            cls_buf.append(cls)
            p0_buf.append(p0)
            p1_buf.append(p1)
            p2_buf.append(p2)
            p3_buf.append(p3)
            index_buf.append(i)
            i += 1

        except Exception as e:
            print("UART error:", e)
            time.sleep(0.1)


threading.Thread(target=uart_reader, daemon=True).start()

# ---------- DASH APP ----------
app = Dash(__name__)
app.title = "AI Inference Live Dashboard"

app.layout = html.Div([
    html.H2("AI Inference Dashboard", style={'textAlign': 'center'}),

    html.H3(id='predicted-class-display',
            style={'textAlign': 'center',
                   'fontWeight': 'bold',
                   'color': '#2a3f5f'}),

    html.Div([
        dcc.Graph(id='live-graph', style={'width': '48%', 'display': 'inline-block'}),
        dcc.Graph(id='raw-graph', style={'width': '48%', 'display': 'inline-block'}),
    ], style={'display': 'flex', 'justifyContent': 'space-between'}),

    html.Div([
        html.Button("💾 Save Current Raw Data to CSV", id='save-button', n_clicks=0,
                    style={'margin': '20px auto', 'display': 'block',
                           'fontSize': '18px', 'padding': '10px 25px', 'backgroundColor': '#4CAF50',
                           'color': 'white', 'border': 'none', 'borderRadius': '6px', 'cursor': 'pointer'}),
        html.Div(id='save-status', style={'textAlign': 'center', 'fontWeight': 'bold', 'color': '#2a3f5f'})
    ]),

    dcc.Interval(id='interval', interval=REFRESH_INTERVAL_MS, n_intervals=0),
], style={'width': '95%', 'margin': 'auto'})


# ---------- CALLBACKS ----------
@app.callback(
    Output('live-graph', 'figure'),
    Output('raw-graph', 'figure'),
    Output('predicted-class-display', 'children'),
    Input('interval', 'n_intervals')
)
def update_graph(_):
    if len(index_buf) < 2:
        return go.Figure(), go.Figure(), "Waiting for data..."

    # Get data
    x = np.array(index_buf)[-SAMPLES_TO_SHOW:]
    p0 = np.array(p0_buf)[-SAMPLES_TO_SHOW:]
    p1 = np.array(p1_buf)[-SAMPLES_TO_SHOW:]
    p2 = np.array(p2_buf)[-SAMPLES_TO_SHOW:]
    p3 = np.array(p3_buf)[-SAMPLES_TO_SHOW:]
    pred_class = int(cls_buf[-1]) if len(cls_buf) else -1

    # --- Class mapping ---
    class_map = {0: "LEFT", 1: "RIGHT", 2: "UP", 3: "DOWN", 4: "BACKGROUND"}
    class_name = class_map.get(pred_class, f"Unknown ({pred_class})")

    # --- Probability Plot ---
    prob_fig = go.Figure()
    prob_fig.add_trace(go.Scatter(x=x, y=p0, name="LEFT", line=dict(color="#1f77b4", width=2)))
    prob_fig.add_trace(go.Scatter(x=x, y=p1, name="RIGHT", line=dict(color="#ff7f0e", width=2)))
    prob_fig.add_trace(go.Scatter(x=x, y=p2, name="UP", line=dict(color="#2ca02c", width=2)))
    prob_fig.add_trace(go.Scatter(x=x, y=p3, name="DOWN", line=dict(color="#4cF02c", width=2)))

    prob_fig.update_layout(
        height=500,
        template="plotly_white",
        hovermode="x unified",
        margin=dict(l=60, r=40, t=40, b=60),
        legend=dict(orientation="h", yanchor="bottom", y=100, xanchor="right", x=1),
        xaxis=dict(title="Sample Index"),
        yaxis=dict(title="Softmax Probability", range=[0, 1]),
    )

    # --- Raw Sensor Plot ---
    latest_raw = np.array(raw_buf[-1])
    raw_fig = go.Figure()
    raw_fig.add_trace(go.Scatter(
        y=latest_raw, mode='lines', line=dict(color="#9467bd", width=2), name="Raw Signal"
    ))

    raw_fig.update_layout(
        height=500,
        template="plotly_white",
        hovermode="x unified",
        margin=dict(l=60, r=40, t=40, b=60),
        xaxis=dict(title="Sample Index (128 per frame)"),
        yaxis=dict(title="Raw Sensor Value", range=[0, 255]),
    )

    return prob_fig, raw_fig, f"Detected Motion (Predicted Class): {class_name}"


@app.callback(
    Output('save-status', 'children'),
    Input('save-button', 'n_clicks'),
    prevent_initial_call=True
)
def save_csv(n_clicks):
    """Triggered when the user presses the save button."""
    if len(raw_buf) == 0:
        return "⚠ No raw data to save yet."

    latest_raw = np.array(raw_buf[-1]).tolist()
    latest_raw = latest_raw[0:125]
    filename = f"pir_data_{n_clicks}.csv"

    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([f"Raw Data Frame #{n_clicks}"])
        writer.writerow(latest_raw)

    return f"✅ Saved current raw data to {filename}"


# ---------- MAIN ----------
if __name__ == '__main__':
    print("Starting dashboard at http://127.0.0.1:8050")
    app.run(debug=False)

