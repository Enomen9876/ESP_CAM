#include "CAM_server.h"

#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define PART_BOUNDARY "123456789000000000000987654321"
#define CAMERA_MODEL_AI_THINKER

// ========================
// Camera pin definition
// ========================

#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#else
  #error "Camera model not selected"
#endif

// ========================
// Globals
// ========================

static bool stream_client_connected = false;
static httpd_handle_t stream_httpd = NULL;
static String debugLog = "";

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ========================
// Debug
// ========================

void addDebug(const String& msg) {
  debugLog += msg + "\n";
  if (debugLog.length() > 3000) {
    debugLog.remove(0, 1000);
  }
}

// ========================
// HTTP Handlers
// ========================

static esp_err_t status_handler(httpd_req_t *req) {
  String status = "";
  status += "IP: " + WiFi.localIP().toString() + "<br>";
  status += "RSSI: " + String(WiFi.RSSI()) + " dBm<br>";
  status += "Heap: " + String(ESP.getFreeHeap()) + "<br>";
  status += "CPU Freq: " + String(getCpuFrequencyMhz()) + " MHz<br>";

  httpd_resp_send(req, status.c_str(), status.length());
  return ESP_OK;
}

static esp_err_t debug_handler(httpd_req_t *req) {
  httpd_resp_send(req, debugLog.c_str(), debugLog.length());
  return ESP_OK;
}

static httpd_req_t* current_client = nullptr;

static esp_err_t stream_handler(httpd_req_t *req) {
    static httpd_req_t* current_client = nullptr;

    // Disconnect previous client if any
    if (current_client && current_client != req) {
        httpd_sess_trigger_close(current_client, 500);
        addDebug("Previous client disconnected for new one");
    }
    current_client = req;
    stream_client_connected = true;

    esp_err_t res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        stream_client_connected = false;
        current_client = nullptr;
        return res;
    }

    char part_buf[64];

    while (true) {
        camera_fb_t *fb = nullptr;
        int retries = 0;
        while (retries < 3) {
            fb = esp_camera_fb_get();
            if (fb) break;
            addDebug("Camera capture failed, retrying...");
            vTaskDelay(50);
            retries++;
        }
        if (!fb) {
            addDebug("Camera capture failed, skipping frame");
            vTaskDelay(50);
            continue; // try next frame
        }

        size_t jpg_len = fb->len;
        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, jpg_len);

        // Send multipart frame
        if (httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK ||
            httpd_resp_send_chunk(req, (const char*)fb->buf, jpg_len) != ESP_OK ||
            httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY)) != ESP_OK) 
        {
            addDebug("Client disconnected or send failed");
            esp_camera_fb_return(fb);
            break;
        }

        esp_camera_fb_return(fb);
        vTaskDelay(10);  // give other tasks CPU time
    }

    stream_client_connected = false;
    current_client = nullptr;
    return ESP_OK;
}

// ========================
// Public Functions
// ========================

void initCamera() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    addDebug("Camera init failed");
  }
}

static const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Monitor</title>
  <meta charset="UTF-8">
  <style>
    body { font-family: Arial; background:#111; color:#eee; text-align:center; margin:0; }
    h1 { color:#00ffcc; margin:15px; }

    .nav {
      display:flex;
      justify-content:center;
      background:#000;
      padding:10px;
      gap:20px;
    }

    .nav button {
      background:#222;
      color:#eee;
      border:none;
      padding:10px 20px;
      border-radius:6px;
      cursor:pointer;
      font-size:16px;
    }

    .nav button:hover { background:#333; }

    .active-dot {
      font-size:14px;
      margin-left:6px;
    }

    .section {
      display:none;
      padding:20px;
    }
    
    img {
      width: 640px;
      border-radius:10px;
      transform: rotate(90deg);
      transform-origin: center center;
    }
    pre { text-align:left; background:#000; padding:10px; height:300px; overflow:auto; }

  </style>
</head>
<body>

<h1>ESP32-CAM Monitor</h1>

<div class="nav">
  <button onclick="showSection('stream')">
    STREAM <span id="streamDot" class="active-dot">🔴</span>
  </button>
  <button onclick="showSection('status')">
    STATUS <span id="statusDot" class="active-dot">🔴</span>
  </button>
  <button onclick="showSection('debug')">
    DEBUG <span id="debugDot" class="active-dot">🔴</span>
  </button>
</div>

<div id="stream" class="section">
  <h2>Live Stream</h2>
  <img id="streamImg" src="/stream">
</div>

<div id="status" class="section">
  <h2>Device Info</h2>
  <div id="info">Loading...</div>
</div>

<div id="debug" class="section">
  <h2>Debug Log</h2>
  <pre id="debugText"></pre>
</div>

<script>
let lastStatus = 0;
let lastDebug = 0;
let lastStream = Date.now();

function showSection(id){
  document.querySelectorAll('.section').forEach(s => s.style.display='none');
  document.getElementById(id).style.display='block';
}

// ===== STATUS =====
function updateStatus(){
  fetch('/status')
    .then(r => r.text())
    .then(t => {
      document.getElementById('info').innerHTML = t;
      lastStatus = Date.now();
    })
    .catch(()=>{});
}

// ===== DEBUG =====
function updateDebug(){
  fetch('/debug')
    .then(r => r.text())
    .then(t => {
      document.getElementById('debugText').innerHTML = t;
      lastDebug = Date.now();
    })
    .catch(()=>{});
}

// ===== STREAM ACTIVITY =====
document.getElementById("streamImg").onload = function(){
  lastStream = Date.now();
};

// ===== ACTIVE DOT UPDATE =====
function updateDots(){
  let now = Date.now();

  document.getElementById("statusDot").textContent =
    (now - lastStatus < 5000) ? "🟢" : "🔴";

  document.getElementById("debugDot").textContent =
    (now - lastDebug < 5000) ? "🟢" : "🔴";

  document.getElementById("streamDot").textContent =
    (now - lastStream < 5000) ? "🟢" : "🔴";
}

// intervals
setInterval(updateStatus, 2000);
setInterval(updateDebug, 2000);
setInterval(updateDots, 1000);

// default screen
showSection('stream');
updateStatus();
updateDebug();

</script>

</body>
</html>
)rawliteral";

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = [](httpd_req_t *req){
      httpd_resp_set_type(req, "text/html");
      httpd_resp_send(req, index_html, strlen(index_html));
      return ESP_OK;
    },
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler,
    .user_ctx = NULL
  };

  httpd_uri_t debug_uri = {
    .uri = "/debug",
    .method = HTTP_GET,
    .handler = debug_handler,
    .user_ctx = NULL
  };

  while (stream_httpd == NULL) {
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &index_uri);
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        httpd_register_uri_handler(stream_httpd, &status_uri);
        httpd_register_uri_handler(stream_httpd, &debug_uri);
        addDebug("HTTP server started");
        break;
    } else {
        addDebug("Failed to start HTTP server, retrying...");
        vTaskDelay(3000);
    }
  }
}