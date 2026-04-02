from arduino.app_utils import *

def log_scd_data(data):
    """Receive SCD30 data from MCU and log it."""
    try:
        co2, temp_c, humidity = data.split(",")
        temp_c = float(temp_c)
        temp_f = (temp_c * 9.0 / 5.0) + 32.0
        print(f"{temp_f:.1f}°F ({temp_c:.1f}°C)  {float(humidity):.1f}%  {float(co2):.1f} ppm")
    except Exception as e:
        print(f"Error parsing data: {e}")

Bridge.provide("log_scd_data", log_scd_data)
App.run()
