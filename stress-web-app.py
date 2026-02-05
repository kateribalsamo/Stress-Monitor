import streamlit as st
import asyncio
from bleak import BleakScanner, BleakClient

# ===== BLE UUIDs =====
SERVICE_UUID = "12345678-1234-1234-1234-1234567890ab"
HR_UUID = "2A37"
HRV_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26b9"
TEMP_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
GSR_UUID = "bada8f01-1111-2222-3333-abcdefabcdef"

# ===== Stress score calculator =====
def calculate_stress_score(hr, hrv, temp, gsr):
    score = 0
    if hr > 100: score += 25
    elif hr > 90: score += 20
    elif hr > 70: score += 10

    if hrv < 20: score += 25
    elif hrv < 40: score += 20
    elif hrv < 60: score += 10

    if temp < 90: score += 25
    elif temp < 93: score += 20
    elif temp < 95: score += 10

    if gsr > 6.0: score += 25
    elif gsr > 4.0: score += 20
    elif gsr > 2.0: score += 10

    return min(score, 100)

# ===== Helper function to parse raw BLE values =====
def parse(data):
    try:
        s = data.decode("utf-8").strip()
        print(f"📥 Raw data: {s}")
        return float(s)
    except Exception as e:
        print(f"❌ Parse error: {e}")
        return 0.0

# ===== Read BLE characteristic with retries =====
async def read_char(client, uuid, label):
    for attempt in range(3):
        try:
            data = await client.read_gatt_char(uuid)
            if data:
                return parse(data)
        except Exception as e:
            print(f"🔁 Retry {label}: {e}")
        await asyncio.sleep(1)
    return 0.0

# ===== Connect and fetch BLE data =====
async def get_ble_data():
    print("🔍 Scanning for BLE devices...")
    devices = await BleakScanner.discover()

    for d in devices:
        if d.name and "StressMonitor" in d.name:
            print(f"✅ Found device: {d.name} - {d.address}")
            async with BleakClient(d.address) as client:
                await client.connect()
                await asyncio.sleep(1.5)  # Ensure ESP32 has time to set values

                hr = await read_char(client, HR_UUID, "HR")
                hrv = await read_char(client, HRV_UUID, "HRV")
                temp = await read_char(client, TEMP_UUID, "Temp")
                gsr = await read_char(client, GSR_UUID, "GSR")

                return hr, hrv, temp, gsr

    print("❌ No StressMonitor device found.")
    return None

# ===== Streamlit Layout =====
st.set_page_config(page_title="Stress Monitor", layout="centered")
st.title("📊 Real-Time Stress Scanner")

status = st.empty()
score_block = st.empty()
col1, col2 = st.columns(2)
col3, col4 = st.columns(2)

if st.button("📶 Get Live Data 📶"):
    try:
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        result = loop.run_until_complete(get_ble_data())

        if result is None:
            st.error("⚠️ Could not find your ESP32. Make sure it's advertising and nearby.")
        else:
            hr, hrv, temp, gsr = result
            stress = calculate_stress_score(hr, hrv, temp, gsr)

            # Stress label & color
            if stress >= 90:
                color, label = "red", "😱 High Stress"
            elif stress >= 60:
                color, label = "orange", "😟 Moderate Stress"
            elif stress >= 30:
                color, label = "gold", "😐 Mild Stress"
            else:
                color, label = "green", "😌 Relaxed"

            score_block.markdown(f"### Stress Score: `{stress}/100`")
            status.markdown(f"<h3 style='color:{color}'>{label}</h3>", unsafe_allow_html=True)

            col1.metric("❤️ Heart Rate", f"{hr:.1f} BPM")
            col2.metric("💓 HRV", f"{hrv:.1f} ms")
            col3.metric("🌡️ Temp", f"{temp:.1f} °F")
            col4.metric("💧 GSR", f"{gsr:.2f} µS")

    except Exception as e:
        st.error(f"❌ Error reading BLE data: {e}")

