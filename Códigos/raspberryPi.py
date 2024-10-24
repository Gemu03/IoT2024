import paho.mqtt.client as mqtt
import requests

THINGSPEAK_API_KEY = "YOUR_THINGSPEAK_WRITE_API_KEY"
THINGSPEAK_UPDATE_URL = "https://api.thingspeak.com/update"

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode()
        print(f"Mensaje recibido: {payload}")
        values = payload.split(", ")
        temp_ext = float(values[0].split("=")[1])
        temp_axil = float(values[1].split("=")[1])
        hum = float(values[2].split("=")[1])

        data = {
            'api_key': THINGSPEAK_API_KEY,
            'field1': temp_ext,
            'field2': temp_axil,
            'field3': hum
        }
        response = requests.post(THINGSPEAK_UPDATE_URL, data=data)

        if response.status_code == 200:
            print("Datos enviados a ThinkSpeak exitosamente.")
        else:
            print(f"Error enviando datos a ThinkSpeak: {response.status_code}")

    except Exception as e:
        print(f"Error procesando mensaje MQTT: {e}")

def on_connect(client, userdata, flags, rc):
    print(f"Conectado al broker MQTT con código de resultado {rc}")
    client.subscribe("incubadora/data")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("localhost", 1883, 60)

client.loop_forever()