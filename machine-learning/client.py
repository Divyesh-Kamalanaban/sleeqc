import streamlit as st
import socket
import time
import binascii
import os

# --- PQC Imports ---
try:
    from pqcrypto.sign import ml_dsa_44, ml_dsa_65, ml_dsa_87
except ImportError:
    st.error("❌ Missing pqcrypto library. Install with: pip install pqcrypto")
    st.stop()

# ======================================================
# Utility Functions
# ======================================================

def send_to_esp32(ip, port, data_to_send):
    """Send message to ESP32 and receive response."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(15)
            st.write(f"Connecting to {ip}:{port}...")
            s.connect((ip, port))

            s.sendall(data_to_send)
            s.shutdown(socket.SHUT_WR)

            response = b""
            while True:
                chunk = s.recv(16384)
                if not chunk:
                    break
                response += chunk
        return response.decode("utf-8", errors="ignore").strip()
    except Exception as e:
        st.error(f"Connection error: {e}")
        return None


def parse_response(response_str):
    """Parse ESP32 response in the format:
    ALG:ML-DSA-87|SIG:...|PK:...
    """
    try:
        parts = response_str.split("|")
        data = {}
        for part in parts:
            if ":" in part:
                k, v = part.split(":", 1)
                data[k.strip()] = v.strip()
        return data
    except Exception as e:
        st.error(f"Parse error: {e}")
        return None


def verify_signature(pk_bytes, sig_bytes, msg_bytes, algo):
    """Verify signature based on detected algorithm."""
    try:
        if algo == 'ML-DSA-87' or len(pk_bytes) == 2592:
            ml_dsa_87.verify(pk_bytes, msg_bytes, sig_bytes)
            return True, "ML-DSA-87 verification succeeded"
        elif algo == 'ML-DSA-65' or len(pk_bytes) == 1952:
            ml_dsa_65.verify(pk_bytes, msg_bytes, sig_bytes)
            return True, "ML-DSA-65 verification succeeded"
        elif algo == 'ML-DSA-44' or len(pk_bytes) == 1312:
            ml_dsa_44.verify(pk_bytes, msg_bytes, sig_bytes)
            return True, "ML-DSA-44 verification succeeded"
        else:
            return False, f"Unknown algorithm or invalid public key length ({len(pk_bytes)})."
    except Exception as e:
        return False, f"Verification failed: {e}"
# ======================================================
# Streamlit UI
# ======================================================

st.set_page_config(page_title="SleeQC PQC Client", page_icon="🔐", layout="wide")
st.title("⚡ SleeQC: Post-Quantum Signature Client & Verifier 🔐")

# Connection Settings
with st.sidebar:
    st.header("🔌 ESP32 Connection")
    esp_ip = st.text_input("ESP32 IP Address", "192.168.137.180")
    esp_port = st.number_input("ESP32 Port", 1, 65535, 8080)

# Message Input
st.header("✉️ Message to Sign")
tab1, tab2 = st.tabs(["Custom Text", "Random Bytes"])

data_to_send = None

with tab1:
    msg = st.text_area("Enter your message:", "Hello from SleeQC Client!")
    if st.button("Sign Custom Message"):
        data_to_send = msg.encode()

with tab2:
    data_len = st.slider("Random data size (bytes)", 16, 1024, 128, 16)
    if st.button("Sign Random Data"):
        random_data = os.urandom(data_len)
        st.code(binascii.hexlify(random_data).decode(), language="text")
        data_to_send = random_data

# ======================================================
# Signing + Verification
# ======================================================

if data_to_send:
    st.divider()
    st.header("📈 Signing Results")

    with st.spinner("Communicating with ESP32..."):
        start = time.time()
        resp = send_to_esp32(esp_ip, esp_port, data_to_send)
        elapsed = time.time() - start

    if not resp:
        st.stop()

    st.metric("Total Round-Trip Time", f"{elapsed:.3f} sec")

    parsed = parse_response(resp)
    if not parsed:
        st.stop()

    algo = parsed.get("ALG", "Unknown")
    sig_hex = parsed.get("SIG", "")
    pk_hex = parsed.get("PK", "")

    if not sig_hex or not pk_hex:
        st.error("ESP32 did not return valid signature or public key.")
        st.text_area("Raw Response", resp)
        st.stop()

    sig_bytes = binascii.unhexlify(sig_hex)
    pk_bytes = binascii.unhexlify(pk_hex)

    col1, col2 = st.columns(2)
    col1.metric("Algorithm Reported by ESP32", algo)
    col2.metric("Public Key Length", f"{len(pk_bytes)} bytes")

    st.write(f"Signature Length: {len(sig_bytes)} bytes")

    # ======================================================
    # Verification
    # ======================================================
    st.subheader("🔐 Signature Verification")

    verified, info = verify_signature(pk_bytes, sig_bytes, data_to_send, algo)
    if verified:
        st.success("✅ Signature VALID — cryptographically verified.")
    else:
        st.error(f"❌ {info}")

    # Debug info
    st.caption(f"Debug → PK: {len(pk_bytes)}, SIG: {len(sig_bytes)}")

    # ======================================================
    # Raw Hex Data
    # ======================================================
    with st.expander("📜 Raw Signature (Hex)"):
        st.code(sig_hex, language="text")

    with st.expander("🔑 Raw Public Key (Hex)"):
        st.code(pk_hex, language="text")

st.divider()
st.caption("💡 SleeQC Client v1.1 — Supports ML-DSA-44/65/87 (Dilithium2/3/5)")
