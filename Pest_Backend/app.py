import os
from datetime import datetime
from flask import Flask, request, jsonify, render_template_string
import requests
import cloudinary
import cloudinary.uploader
import cloudinary.api
from dotenv import load_dotenv

from pathlib import Path

env_path = Path(__file__).resolve().parent / ".env"
load_dotenv(dotenv_path=env_path, override=True)

app = Flask(__name__)

cloud_name = os.getenv("CLOUD_NAME", "").strip()
api_key = os.getenv("API_KEY", "").strip()
api_secret = os.getenv("API_SECRET", "").strip()

if not all([cloud_name, api_key, api_secret]):
    raise RuntimeError("Cloudinary credentials missing!")

cloudinary.config(
    cloud_name=cloud_name,
    api_key=api_key,
    api_secret=api_secret,
    secure=True
)

print("Cloudinary Name:", cloud_name)
print("API Key Loaded:", bool(api_key))
print("API Secret Loaded:", bool(api_secret))
FIREBASE_HOST = os.getenv("FIREBASE_HOST")

@app.route("/", methods=["GET"])
def home():
    return render_template_string("""
    <h2>Pest Detection Backend Active</h2>
    <p><a href="/gallery">Open Gallery</a></p>
    <p><a href="/frequency">Change Capture Frequency</a></p>
    """)

@app.route("/upload", methods=["POST"])
def upload():
    try:
        image_data = request.get_data()
        if not image_data:
            return jsonify({"status": "error", "message": "No data received"}), 400

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        public_id = f"pest_{timestamp}"

        response = cloudinary.uploader.upload(
            image_data,
            folder="pest_detection",
            public_id=public_id,
            resource_type="image"
        )

        return jsonify({
            "status": "success",
            "url": response.get("secure_url"),
            "public_id": response.get("public_id")
        }), 200

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

@app.route("/gallery", methods=["GET"])
def gallery():
    try:
        res = cloudinary.api.resources(
            type="upload",
            prefix="pest_detection/",
            max_results=100
        )
        images = res.get("resources", [])
        images.sort(key=lambda x: x.get("created_at", ""), reverse=True)
    except Exception as e:
        images = []

    html = """
    <html>
    <head><title>Pest Detection Gallery</title></head>
    <body style="font-family: Arial; padding: 20px;">
        <h2>Captured Images Gallery</h2>
        <p><a href="/frequency">Go to Frequency Settings</a></p>
        <div style="display: flex; flex-wrap: wrap; gap: 15px;">
        {% for img in images %}
            <div style="border: 1px solid #ccc; padding: 10px; border-radius: 8px;">
                <img src="{{ img.secure_url }}" width="240" style="display: block; margin-bottom: 5px;"/>
                <small>{{ img.created_at }}</small>
            </div>
        {% else %}
            <p>No images uploaded yet.</p>
        {% endfor %}
        </div>
    </body>
    </html>
    """
    return render_template_string(html, images=images)

@app.route("/frequency", methods=["GET", "POST"])
def frequency():
    url = f"https://{FIREBASE_HOST}/frequency.json"
    message = ""

    if request.method == "POST":
        new_freq = request.form.get("frequency")
        if new_freq and new_freq.isdigit() and int(new_freq) > 0:
            res = requests.put(url, json=int(new_freq))
            if res.status_code == 200:
                message = f"Frequency updated successfully to {new_freq}!"
            else:
                message = f"Failed to update Firebase: {res.text}"
        else:
            message = "Please enter a valid positive integer."

    current_val = "Unknown"
    try:
        res = requests.get(url, timeout=5)
        if res.status_code == 200:
            current_val = res.json()
    except Exception as e:
        current_val = f"Error: {e}"

    html = """
    <html>
    <head><title>Capture Frequency</title></head>
    <body style="font-family: Arial; padding: 20px;">
        <h2>Capture Frequency Settings</h2>
        <p><strong>Current Images Per Day (Firebase):</strong> {{ current_val }}</p>
        {% if message %}<p style="color: green;">{{ message }}</p>{% endif %}
        <form method="POST">
            <label>Set Images Per Day (e.g. 2, 4, 12):</label><br><br>
            <input type="number" name="frequency" min="1" max="48" required style="padding: 5px;"/>
            <button type="submit" style="padding: 5px 10px;">Update</button>
        </form>
        <br>
        <a href="/gallery">View Gallery</a>
    </body>
    </html>
    """
    return render_template_string(html, current_val=current_val, message=message)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)