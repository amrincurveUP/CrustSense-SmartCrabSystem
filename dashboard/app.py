"""Smart Aquaculture live GUI dashboard."""

from __future__ import annotations

import time

import pandas as pd
import plotly.graph_objects as go
import streamlit as st
from plotly.subplots import make_subplots
from serial_reader import SerialBridge

st.set_page_config(
    page_title="Aquaculture Monitor",
    page_icon="🦐",
    layout="wide",
)

COLORS = {
    "ACTIVE": "#10b981",
    "CONNECTED": "#f59e0b",
    "OFF": "#ef4444",
}

PIN_MAP = {
    "Temperature (NTC 10k)": "GPIO4",
    "Turbidity (TS-300B)": "GPIO7",
    "Water level (HW-038)": "GPIO8",
}


def init_state() -> None:
    if "bridge" not in st.session_state:
        st.session_state.bridge = SerialBridge()


def status_dot(state: str) -> str:
    color = COLORS.get(state, "#64748b")
    return f"<span style='color:{color};font-size:1.4rem;'>●</span>"


def render_metric(title: str, gpio: str, value: str, detail: str, state: str) -> None:
    border = COLORS.get(state, "#64748b")
    st.markdown(
        f"""
        <div style="background:linear-gradient(145deg,#1e293b,#0f172a);
                    border:1px solid #334155;border-top:4px solid {border};
                    border-radius:14px;padding:20px;min-height:155px;">
          <div style="display:flex;justify-content:space-between;align-items:center;">
            <span style="color:#94a3b8;font-size:0.95rem;">{title}</span>
            <span style="color:#64748b;font-size:0.8rem;">{gpio}</span>
          </div>
          <div style="color:#f8fafc;font-size:2.2rem;font-weight:700;margin:12px 0 6px 0;">
            {value}
          </div>
          <div style="color:#cbd5e1;font-size:0.95rem;">{detail}</div>
          <div style="margin-top:10px;color:{border};font-weight:600;">{state}</div>
        </div>
        """,
        unsafe_allow_html=True,
    )


def level_gauge(pct: int, label: str) -> go.Figure:
    fig = go.Figure(
        go.Indicator(
            mode="gauge+number",
            value=pct,
            number={"suffix": "%", "font": {"size": 36, "color": "#f8fafc"}},
            title={"text": f"Water Level · {label}", "font": {"size": 16, "color": "#94a3b8"}},
            gauge={
                "axis": {"range": [0, 100], "tickcolor": "#64748b"},
                "bar": {"color": "#34d399"},
                "bgcolor": "#1e293b",
                "borderwidth": 0,
                "steps": [
                    {"range": [0, 25], "color": "#334155"},
                    {"range": [25, 60], "color": "#1e3a5f"},
                    {"range": [60, 100], "color": "#064e3b"},
                ],
            },
        )
    )
    fig.update_layout(
        height=260,
        margin=dict(l=20, r=20, t=50, b=10),
        paper_bgcolor="#0f172a",
        font={"color": "#f8fafc"},
    )
    return fig


def trend_chart(df: pd.DataFrame) -> go.Figure:
    fig = make_subplots(
        rows=3,
        cols=1,
        shared_xaxes=True,
        vertical_spacing=0.06,
        subplot_titles=("Temperature (°C)", "Turbidity (V)", "Water Level (%)"),
    )

    if not df.empty:
        x = pd.to_datetime(df["time"], unit="s")
        fig.add_trace(
            go.Scatter(x=x, y=df["temp"], mode="lines+markers", name="Temp",
                       line=dict(color="#38bdf8", width=2), marker=dict(size=4)),
            row=1, col=1,
        )
        fig.add_trace(
            go.Scatter(x=x, y=df["turb"], mode="lines+markers", name="Turbidity",
                       line=dict(color="#c084fc", width=2), marker=dict(size=4)),
            row=2, col=1,
        )
        fig.add_trace(
            go.Scatter(x=x, y=df["level"], mode="lines+markers", name="Level",
                       fill="tozeroy", line=dict(color="#34d399", width=2), marker=dict(size=4)),
            row=3, col=1,
        )

    fig.update_layout(
        height=580,
        template="plotly_dark",
        showlegend=False,
        paper_bgcolor="#0f172a",
        plot_bgcolor="#111827",
        margin=dict(l=10, r=10, t=40, b=10),
    )
    fig.update_xaxes(gridcolor="#334155")
    fig.update_yaxes(gridcolor="#334155")
    return fig


def main() -> None:
    init_state()
    bridge: SerialBridge = st.session_state.bridge

    st.markdown(
        """
        <style>
        .block-container { padding-top: 1rem; max-width: 1200px; }
        [data-testid="stSidebar"] { background: #0f172a; }
        </style>
        """,
        unsafe_allow_html=True,
    )

    st.title("🦐 Smart Aquaculture Monitor")
    st.caption("Live readings · Thermistor GPIO4 · Turbidity GPIO7 · Water level GPIO8")

    with st.sidebar:
        st.subheader("ESP32 Connection")
        ports = bridge.list_ports()
        port = st.selectbox("USB port", ports or ["No port found"], disabled=not ports)
        baud = st.selectbox("Baud rate", [115200], index=0)

        if st.button("Connect", use_container_width=True, disabled=not ports):
            try:
                bridge.baud = baud
                bridge.connect(port)
                st.success(f"Connected to {port}")
            except Exception as exc:  # noqa: BLE001
                st.error(str(exc))

        if st.button("Disconnect", use_container_width=True):
            bridge.disconnect()

        if st.button("Clear charts", use_container_width=True):
            bridge.clear_history()

        st.divider()
        st.markdown("**Before connecting**")
        st.markdown(
            "- Flash latest firmware\n"
            "- Close VS Code serial monitor\n"
            "- Wire all 3 sensors\n"
            "- Plug ESP32 USB"
        )

        st.divider()
        st.markdown("**Your pin map**")
        for name, gpio in PIN_MAP.items():
            st.markdown(f"- {name}: **{gpio}**")

        if bridge.is_connected:
            st.success("Receiving data")
            st.caption(f"Samples: {bridge.lines_read}")
        else:
            st.warning("Not connected")

        if bridge.last_error:
            st.error(bridge.last_error)

    latest = bridge.latest
    if latest is None:
        st.info("Connect the ESP32 to see live sensor readings and trend charts.")
        time.sleep(1)
        st.rerun()
        return

    summary_l, summary_r = st.columns([3, 1])
    with summary_l:
        st.markdown(
            f"### {status_dot(latest.temp_st)} {status_dot(latest.turb_st)} "
            f"{status_dot(latest.level_st)} "
            f"&nbsp; **{latest.connected}/3** sensors connected · "
            f"**{latest.active}/3** active",
            unsafe_allow_html=True,
        )
    with summary_r:
        st.caption(f"Updated every ~2 s · {bridge.lines_read} samples")

    c1, c2, c3 = st.columns(3)
    temp_val = f"{latest.temp_c:.1f} °C" if latest.temp_c is not None else "—"
    with c1:
        render_metric(
            "Temperature",
            PIN_MAP["Temperature (NTC 10k)"],
            temp_val,
            f"ADC {latest.temp_v:.3f} V",
            latest.temp_st,
        )
    with c2:
        render_metric(
            "Turbidity",
            PIN_MAP["Turbidity (TS-300B)"],
            f"{latest.turb_v:.3f} V",
            f"Raw {latest.turb_raw}",
            latest.turb_st,
        )
    with c3:
        render_metric(
            "Water Level",
            PIN_MAP["Water level (HW-038)"],
            f"{latest.level_pct}%",
            f"{latest.level_label} · raw {latest.level_raw}",
            latest.level_st,
        )

    gauge_col, table_col = st.columns([1, 1])
    with gauge_col:
        st.plotly_chart(level_gauge(latest.level_pct, latest.level_label), use_container_width=True)
    with table_col:
        st.subheader("Current values")
        st.table(
            pd.DataFrame(
                [
                    {"Parameter": "Temperature", "Value": temp_val, "Status": latest.temp_st},
                    {"Parameter": "Turbidity (V)", "Value": f"{latest.turb_v:.3f}", "Status": latest.turb_st},
                    {"Parameter": "Water level", "Value": f"{latest.level_pct}% ({latest.level_label})", "Status": latest.level_st},
                ]
            )
        )

    rows = [
        {"time": r.timestamp, "temp": r.temp_c, "turb": r.turb_v, "level": r.level_pct}
        for r in bridge.history
    ]
    st.subheader("Trend — watch values change as water conditions change")
    st.plotly_chart(trend_chart(pd.DataFrame(rows)), use_container_width=True)

    time.sleep(1.5)
    st.rerun()


if __name__ == "__main__":
    main()
