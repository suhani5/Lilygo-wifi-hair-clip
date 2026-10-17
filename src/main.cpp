#include <AnimatedGIF.h>
#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {
constexpr int TFT_WR = 8;
constexpr int TFT_RD = 9;
constexpr int TFT_RST = 5;
constexpr int TFT_BACKLIGHT = 38;
constexpr int TFT_POWER = 15;

constexpr char AP_SSID[] = "Display-Setup";
constexpr char AP_PASSWORD[] = "display123";
constexpr char DEFAULT_BACKGROUND[] = "#101820";
constexpr char DEFAULT_FOREGROUND[] = "#F2F2F2";
constexpr char GIF_TEMP_PATH[] = "/uploaded.tmp";
constexpr uint8_t MAX_GIF_COUNT = 6;
constexpr size_t MAX_GIF_SIZE = 384 * 1024;

Arduino_DataBus *bus =
    new Arduino_ESP32PAR8Q(7, 6, TFT_WR, TFT_RD, 39, 40, 41, 42, 45, 46, 47,
                           48);
Arduino_GFX *display =
    new Arduino_ST7789(bus, TFT_RST, 1, true, 170, 320, 35, 0, 35, 0);
Preferences preferences;
WebServer server(80);
AnimatedGIF gif;
File gifFile;
File uploadFile;

String title = "Hello from LilyGO";
String message =
    "Connect to the Wi-Fi network, then open 192.168.4.1 to customize this "
    "screen.";
String footer = "Display-Setup | 192.168.4.1";
String background = DEFAULT_BACKGROUND;
String foreground = DEFAULT_FOREGROUND;
String animation = "text";
String activeAnimation;
size_t uploadedBytes = 0;
uint8_t pendingGifSlot = MAX_GIF_COUNT;
String uploadName;
bool fileSystemReady = false;
bool uploadFailed = false;
String uploadError;
uint32_t nextAnimationFrame = 0;
int gifOffsetX = 0;
int gifOffsetY = 0;

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length());
  for (const char character : value) {
    switch (character) {
    case '&':
      escaped += F("&amp;");
      break;
    case '<':
      escaped += F("&lt;");
      break;
    case '>':
      escaped += F("&gt;");
      break;
    case '"':
      escaped += F("&quot;");
      break;
    case '\'':
      escaped += F("&#39;");
      break;
    default:
      escaped += character;
      break;
    }
  }
  return escaped;
}

bool isHexColor(const String &value) {
  if (value.length() != 7 || value[0] != '#') {
    return false;
  }
  for (size_t i = 1; i < value.length(); ++i) {
    if (!isxdigit(static_cast<unsigned char>(value[i]))) {
      return false;
    }
  }
  return true;
}

uint16_t color565(const String &value) {
  const uint32_t rgb = strtoul(value.c_str() + 1, nullptr, 16);
  return display->color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

String gifPath(uint8_t slot) {
  return "/anim" + String(slot) + ".gif";
}

String gifNameKey(uint8_t slot) {
  return "gif" + String(slot);
}

bool hasGif(uint8_t slot) {
  return fileSystemReady && slot < MAX_GIF_COUNT &&
         LittleFS.exists(gifPath(slot));
}

bool isGifAnimation(const String &value) {
  if (!value.startsWith("gif") || value.length() != 4 ||
      value[3] < '0' || value[3] >= '0' + MAX_GIF_COUNT) {
    return false;
  }
  return hasGif(value[3] - '0');
}

uint8_t findFreeGifSlot() {
  for (uint8_t slot = 0; slot < MAX_GIF_COUNT; ++slot) {
    if (!hasGif(slot)) {
      return slot;
    }
  }
  return MAX_GIF_COUNT;
}

void renderTextScreen() {
  display->fillScreen(color565(background));
  display->setTextColor(color565(foreground));
  display->setTextWrap(true);

  display->setTextSize(2);
  display->setCursor(10, 12);
  display->println(title);

  display->setTextSize(2);
  display->setCursor(10, 56);
  display->println(message);

  display->setTextSize(1);
  display->setCursor(10, 151);
  display->print(footer);
}

void drawDotChomper(uint32_t now) {
  const int width = display->width();
  const int height = display->height();
  const int centerY = height / 2;
  const int radius = 29;
  const int x = static_cast<int>((now / 22) % (width + radius * 2)) - radius;
  const bool mouthOpen = ((now / 130) % 2) == 0;

  display->fillScreen(color565(background));
  for (int dotX = 24; dotX < width; dotX += 27) {
    if (dotX > x + radius) {
      display->fillCircle(dotX, centerY, 3, color565(foreground));
    }
  }

  display->fillCircle(x, centerY, radius, 0xFFE0);
  if (mouthOpen) {
    display->fillTriangle(x, centerY, x + radius + 1, centerY - 19,
                          x + radius + 1, centerY + 19,
                          color565(background));
  }
}

void drawWalkingPup(uint32_t now) {
  const int width = display->width();
  const int height = display->height();
  const int x = static_cast<int>((now / 28) % (width + 100)) - 50;
  const int ground = height - 24;
  const int stride = ((now / 140) % 2) ? 8 : -8;
  const uint16_t fur = 0xD69A;
  const uint16_t ear = 0xA145;
  const uint16_t dark = 0x2104;
  const uint16_t cream = 0xFFDF;

  display->fillScreen(color565(background));
  display->drawFastHLine(0, ground + 1, width, color565(foreground));
  display->fillRoundRect(x - 29, ground - 34, 59, 28, 12, fur);
  display->fillCircle(x + 28, ground - 48, 18, fur);
  display->fillRoundRect(x + 15, ground - 67, 13, 31, 6, ear);
  display->fillCircle(x + 44, ground - 44, 5, cream);
  display->fillCircle(x + 46, ground - 44, 2, dark);
  display->fillCircle(x + 54, ground - 38, 4, dark);
  display->fillTriangle(x - 28, ground - 30, x - 45, ground - 45,
                        x - 43, ground - 23, fur);
  display->fillRoundRect(x - 23 + stride, ground - 13, 9, 15, 3, fur);
  display->fillRoundRect(x - 2 - stride, ground - 13, 9, 15, 3, fur);
  display->fillRoundRect(x + 16 + stride, ground - 13, 9, 15, 3, fur);
}

void stopGif() {
  gif.close();
}

void *gifOpenFile(const char *filename, int32_t *fileSize) {
  if (gifFile) {
    gifFile.close();
  }
  gifFile = LittleFS.open(filename, "r");
  if (!gifFile) {
    return nullptr;
  }
  *fileSize = static_cast<int32_t>(gifFile.size());
  return &gifFile;
}

void gifCloseFile(void *handle) {
  auto *file = static_cast<File *>(handle);
  if (file != nullptr && *file) {
    file->close();
  }
}

int32_t gifReadFile(GIFFILE *fileInfo, uint8_t *buffer, int32_t length) {
  auto *file = static_cast<File *>(fileInfo->fHandle);
  if (file == nullptr || !*file || length <= 0) {
    return 0;
  }
  const int32_t bytesRead = static_cast<int32_t>(file->read(buffer, length));
  fileInfo->iPos = static_cast<int32_t>(file->position());
  return bytesRead;
}

int32_t gifSeekFile(GIFFILE *fileInfo, int32_t position) {
  auto *file = static_cast<File *>(fileInfo->fHandle);
  if (file == nullptr || !*file || position < 0 ||
      !file->seek(static_cast<uint32_t>(position))) {
    return -1;
  }
  fileInfo->iPos = static_cast<int32_t>(file->position());
  return fileInfo->iPos;
}

void drawGifLine(GIFDRAW *draw) {
  const int y = gifOffsetY + draw->iY + draw->y;
  if (y < 0 || y >= display->height()) {
    return;
  }

  if (draw->ucDisposalMethod == 2 && draw->ucHasTransparency) {
    const int frameWidth = min(draw->iWidth, 320);
    for (int x = 0; x < frameWidth; ++x) {
      if (draw->pPixels[x] == draw->ucTransparent) {
        draw->pPixels[x] = draw->ucBackground;
      }
    }
    draw->ucHasTransparency = 0;
  }

  const int xStart = gifOffsetX + draw->iX;
  const int lineWidth = min(draw->iWidth, display->width() - xStart);
  if (lineWidth <= 0) {
    return;
  }

  uint16_t pixels[320];
  int x = 0;
  while (x < lineWidth) {
    while (x < lineWidth && draw->ucHasTransparency &&
           draw->pPixels[x] == draw->ucTransparent) {
      ++x;
    }
    const int runStart = x;
    while (x < lineWidth &&
           (!draw->ucHasTransparency ||
            draw->pPixels[x] != draw->ucTransparent)) {
      pixels[x - runStart] = draw->pPalette[draw->pPixels[x]];
      ++x;
    }
    if (x > runStart) {
      display->draw16bitRGBBitmap(xStart + runStart, y, pixels, x - runStart,
                                  1);
    }
  }
}

void discardGifLine(GIFDRAW *) {}

bool openUploadedGif(GIF_DRAW_CALLBACK *drawCallback) {
  if (!isGifAnimation(animation)) {
    return false;
  }
  gif.begin(GIF_PALETTE_RGB565_LE);
  const uint8_t slot = animation[3] - '0';
  const String path = gifPath(slot);
  if (!gif.open(path.c_str(), gifOpenFile, gifCloseFile, gifReadFile,
                gifSeekFile, drawCallback)) {
    return false;
  }
  if (gif.getCanvasWidth() > display->width() ||
      gif.getCanvasHeight() > display->height()) {
    gif.close();
    return false;
  }
  return true;
}

bool validateUploadedGif() {
  gif.begin(GIF_PALETTE_RGB565_LE);
  if (!gif.open(GIF_TEMP_PATH, gifOpenFile, gifCloseFile, gifReadFile,
                gifSeekFile, discardGifLine)) {
    Serial.printf("GIF open/validation failed (decoder error %d).\n",
                  gif.getLastError());
    return false;
  }
  if (gif.getCanvasWidth() > display->width() ||
      gif.getCanvasHeight() > display->height()) {
    Serial.printf("GIF is %dx%d; display is %dx%d.\n", gif.getCanvasWidth(),
                  gif.getCanvasHeight(), display->width(), display->height());
    gif.close();
    return false;
  }
  int frameDelay = 0;
  const int frameResult = gif.playFrame(false, &frameDelay);
  const bool hasFrame = frameResult >= 0 && gif.getLastError() == GIF_SUCCESS;
  if (!hasFrame) {
    Serial.printf("GIF first frame failed (decoder error %d).\n",
                  gif.getLastError());
  }
  gif.close();
  return hasFrame;
}

String makePage() {
  String page = F(
      "<!doctype html><html lang=\"en\"><meta name=\"viewport\" "
      "content=\"width=device-width,initial-scale=1\"><title>Display "
      "Setup</title><style>"
      "body{font:16px system-ui,sans-serif;max-width:36rem;margin:2rem "
      "auto;padding:0 1rem;background:#f4f5f7;color:#17202a}"
      "form{background:white;padding:1.25rem;margin:1rem 0;border-radius:"
      "12px;box-shadow:0 2px 12px #0001}label{display:block;margin:1rem "
      "0 .35rem}input[type=text],textarea,select{box-sizing:border-box;"
      "width:100%;padding:.65rem;font:inherit}input[type=color]{width:4rem;"
      "height:2.5rem}button{margin-top:1.25rem;padding:.7rem 1rem;font:"
      "inherit}small{color:#4b5563}</style><h1>Screen setup</h1>"
      "<form method=\"post\" action=\"/save\">"
      "<label for=\"animation\">What to show</label><select id=\"animation\" "
      "name=\"animation\">");
  const char *modes[] = {"text", "chomper", "pup"};
  const char *labels[] = {"Text screen", "Dot chomper", "Walking pup"};
  for (size_t i = 0; i < 3; ++i) {
    page += F("<option value=\"");
    page += modes[i];
    page += '"';
    if (animation == modes[i]) {
      page += F(" selected");
    }
    page += '>';
    page += labels[i];
    page += F("</option>");
  }
  for (uint8_t slot = 0; slot < MAX_GIF_COUNT; ++slot) {
    if (!hasGif(slot)) {
      continue;
    }
    page += F("<option value=\"gif");
    page += String(slot);
    page += '"';
    if (animation == "gif" + String(slot)) {
      page += F(" selected");
    }
    page += '>';
    page += htmlEscape(preferences.getString(gifNameKey(slot).c_str(),
                                             "Uploaded animation"));
    page += F("</option>");
  }

  page += F(
      "</select><label for=\"title\">Title</label><input id=\"title\" "
      "name=\"title\" type=\"text\" maxlength=\"25\" required value=\"");
  page += htmlEscape(title);
  page += F(
      "\"><label for=\"message\">Message</label><textarea id=\"message\" "
      "name=\"message\" maxlength=\"100\" rows=\"4\" required>");
  page += htmlEscape(message);
  page += F(
      "</textarea><label for=\"footer\">Footer</label><input id=\"footer\" "
      "name=\"footer\" type=\"text\" maxlength=\"48\" required value=\"");
  page += htmlEscape(footer);
  page += F(
      "\"><label for=\"background\">Background</label><input "
      "id=\"background\" name=\"background\" type=\"color\" value=\"");
  page += background;
  page += F(
      "\"><label for=\"foreground\">Text</label><input id=\"foreground\" "
      "name=\"foreground\" type=\"color\" value=\"");
  page += foreground;
  page += F(
      "\"><br><button type=\"submit\">Save settings</button></form>"
      "<h2>Upload an animation</h2><form id=\"upload-form\" method=\"post\" "
      "action=\"/upload\" "
      "enctype=\"multipart/form-data\"><label for=\"gif\">GIF animation "
      "file</label><input id=\"gif\" name=\"gif\" type=\"file\" "
      "accept=\".gif,image/gif\" required><br><button type=\"submit\">"
      "Upload GIF</button><p><small>GIF only, up to 384 KB each, and no larger "
      "than the screen (");
  page += String(display->width());
  page += 'x';
  page += String(display->height());
  page += F("). Up to six can be stored. Original artwork recommended.</small>"
            "</p></form>");
  for (uint8_t slot = 0; slot < MAX_GIF_COUNT; ++slot) {
    if (!hasGif(slot)) {
      continue;
    }
    page += F("<form method=\"post\" action=\"/delete-gif?slot=");
    page += String(slot);
    page += F("\"><p>Stored animation: ");
    page += htmlEscape(preferences.getString(gifNameKey(slot).c_str(),
                                             "Uploaded animation"));
    page += F("</p><button type=\"submit\">Delete this GIF</button></form>");
  }
  page += F(
      "<p id=\"upload-status\" role=\"status\" aria-live=\"polite\"></p>"
      "<script>const f=document.getElementById('upload-form'),"
      "s=document.getElementById('upload-status');f.addEventListener('submit',"
      "e=>{e.preventDefault();const x=new XMLHttpRequest();x.open('POST',"
      "'/upload');x.upload.onprogress=e=>{if(e.lengthComputable)s.textContent="
      "'Uploading '+Math.round(e.loaded/e.total*100)+'%'};x.onload=()=>{"
      "if(x.status>=200&&x.status<300){location.href='/'}else{s.textContent="
      "'Upload failed: '+x.responseText}};x.onerror=()=>{s.textContent="
      "'Upload failed: connection lost. Try again.'};s.textContent="
      "'Starting upload...';x.send(new FormData(f))})</script></html>");
  return page;
}

void handleSave() {
  if (!server.hasArg("title") || !server.hasArg("message") ||
      !server.hasArg("footer") || !server.hasArg("background") ||
      !server.hasArg("foreground") || !server.hasArg("animation")) {
    server.send(400, "text/plain", "Missing one or more settings.");
    return;
  }

  const String newTitle = server.arg("title");
  const String newMessage = server.arg("message");
  const String newFooter = server.arg("footer");
  const String newBackground = server.arg("background");
  const String newForeground = server.arg("foreground");
  const String newAnimation = server.arg("animation");
  if (newTitle.isEmpty() || newMessage.isEmpty() || newFooter.isEmpty() ||
      newTitle.length() > 25 || newMessage.length() > 100 ||
      newFooter.length() > 48 || !isHexColor(newBackground) ||
      !isHexColor(newForeground) ||
      (newAnimation != "text" && newAnimation != "chomper" &&
       newAnimation != "pup" && !isGifAnimation(newAnimation))) {
    server.send(400, "text/plain", "Invalid settings or animation choice.");
    return;
  }

  if (preferences.putString("title", newTitle) == 0 ||
      preferences.putString("message", newMessage) == 0 ||
      preferences.putString("footer", newFooter) == 0 ||
      preferences.putString("background", newBackground) == 0 ||
      preferences.putString("foreground", newForeground) == 0 ||
      preferences.putString("animation", newAnimation) == 0) {
    server.send(500, "text/plain",
                "Could not save settings to the board. Please try again.");
    return;
  }

  title = newTitle;
  message = newMessage;
  footer = newFooter;
  background = newBackground;
  foreground = newForeground;
  animation = newAnimation;
  activeAnimation = "";

  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "Saved");
}

void handleUploadData() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadedBytes = 0;
    pendingGifSlot = findFreeGifSlot();
    uploadName = upload.filename;
    const int slash =
        max(uploadName.lastIndexOf('/'), uploadName.lastIndexOf('\\'));
    if (slash >= 0) {
      uploadName = uploadName.substring(slash + 1);
    }
    const int dot = uploadName.lastIndexOf('.');
    if (dot >= 0) {
      uploadName = uploadName.substring(0, dot);
    }
    String safeName;
    for (size_t i = 0; i < uploadName.length() && safeName.length() < 24; ++i) {
      const char character = uploadName[i];
      if (isalnum(static_cast<unsigned char>(character)) || character == ' ' ||
          character == '-' || character == '_') {
        safeName += character;
      }
    }
    uploadName = safeName.isEmpty() ? "Uploaded animation" : safeName;
    uploadFailed = false;
    String extension = upload.filename;
    extension.toLowerCase();
    if (!fileSystemReady) {
      uploadFailed = true;
      uploadError = "Board flash storage is unavailable.";
    } else if (pendingGifSlot == MAX_GIF_COUNT) {
      uploadFailed = true;
      uploadError = "All six animation slots are full; delete a GIF first.";
    } else if (!extension.endsWith(".gif")) {
      uploadFailed = true;
      uploadError = "Choose a GIF file ending in .gif.";
    }
    if (fileSystemReady) {
      LittleFS.remove(GIF_TEMP_PATH);
      if (!uploadFailed) {
        uploadFile = LittleFS.open(GIF_TEMP_PATH, "w");
        if (!uploadFile) {
          uploadFailed = true;
          uploadError = "Could not create a temporary file in board storage.";
        }
      }
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFailed) {
      return;
    }
    if (uploadedBytes + upload.currentSize > MAX_GIF_SIZE ||
        uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      uploadFailed = true;
      uploadError = uploadedBytes + upload.currentSize > MAX_GIF_SIZE
                        ? "GIF is larger than 384 KB."
                        : "Board flash storage ran out of space while writing.";
      uploadFile.close();
      LittleFS.remove(GIF_TEMP_PATH);
      return;
    }
    uploadedBytes += upload.currentSize;
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    uploadFailed = true;
    uploadError = "The browser or Wi-Fi connection interrupted the upload.";
    if (uploadFile) {
      uploadFile.close();
    }
    if (fileSystemReady) {
      LittleFS.remove(GIF_TEMP_PATH);
    }
  }
}

void handleUploadComplete() {
  if (uploadFailed || uploadedBytes == 0 || uploadedBytes > MAX_GIF_SIZE) {
    if (fileSystemReady) {
      LittleFS.remove(GIF_TEMP_PATH);
    }
    const int status = !fileSystemReady ? 503
                       : pendingGifSlot == MAX_GIF_COUNT ? 507
                                                         : 400;
    const String error = uploadError.isEmpty()
                             ? "No file data was received. Try uploading again."
                             : uploadError;
    server.send(status, "text/plain", error);
    return;
  }

  stopGif();
  activeAnimation = "";
  if (!validateUploadedGif()) {
    LittleFS.remove(GIF_TEMP_PATH);
    server.send(400, "text/plain",
                "GIF is invalid, too large for the display, or could not be "
                "decoded. See the serial monitor for decoder details.");
    return;
  }

  const String destination = gifPath(pendingGifSlot);
  if (!LittleFS.rename(GIF_TEMP_PATH, destination.c_str())) {
    LittleFS.remove(GIF_TEMP_PATH);
    server.send(500, "text/plain",
                "Could not move the validated GIF into animation storage.");
    return;
  }
  const String selected = "gif" + String(pendingGifSlot);
  if (preferences.putString(gifNameKey(pendingGifSlot).c_str(), uploadName) ==
          0 ||
      preferences.putString("animation", selected) == 0) {
    server.send(500, "text/plain",
                "GIF uploaded, but its settings could not be saved.");
    return;
  }
  animation = selected;
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "GIF uploaded and selected.");
}

void handleDeleteGif() {
  if (!server.hasArg("slot")) {
    server.send(400, "text/plain", "Missing animation slot.");
    return;
  }
  const int slot = server.arg("slot").toInt();
  if (slot < 0 || slot >= MAX_GIF_COUNT || !hasGif(slot)) {
    server.send(404, "text/plain", "That uploaded GIF was not found.");
    return;
  }
  stopGif();
  if (!LittleFS.remove(gifPath(slot))) {
    server.send(500, "text/plain", "Could not delete the uploaded GIF.");
    return;
  }
  if (animation == "gif" + String(slot)) {
    animation = "text";
    if (preferences.putString("animation", animation) == 0) {
      server.send(500, "text/plain",
                  "GIF deleted, but the selected animation could not be "
                  "saved.");
      return;
    }
  }
  preferences.remove(gifNameKey(slot).c_str());
  activeAnimation = "";
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "Uploaded GIF deleted.");
}

bool loadSettings() {
  if (!preferences.begin("display", false)) {
    return false;
  }
  title = preferences.getString("title", title);
  message = preferences.getString("message", message);
  footer = preferences.getString("footer", footer);
  background = preferences.getString("background", background);
  foreground = preferences.getString("foreground", foreground);
  animation = preferences.getString("animation", animation);

  if (!isHexColor(background) || !isHexColor(foreground)) {
    Serial.println("Invalid saved colors; using default colors.");
    background = DEFAULT_BACKGROUND;
    foreground = DEFAULT_FOREGROUND;
  }
  if (animation != "text" && animation != "chomper" && animation != "pup" &&
      !isGifAnimation(animation)) {
    animation = "text";
  }
  return true;
}

void updateAnimation(uint32_t now) {
  if (animation != activeAnimation) {
    stopGif();
    activeAnimation = animation;
    nextAnimationFrame = now;
    if (animation == "text") {
      renderTextScreen();
    } else if (isGifAnimation(animation)) {
      if (!openUploadedGif(drawGifLine)) {
        Serial.println("Could not open the uploaded GIF; showing text screen.");
        animation = "text";
        activeAnimation = "text";
        renderTextScreen();
        return;
      }
      gifOffsetX = (display->width() - gif.getCanvasWidth()) / 2;
      gifOffsetY = (display->height() - gif.getCanvasHeight()) / 2;
      display->fillScreen(color565(background));
    }
  }

  if (now < nextAnimationFrame) {
    return;
  }
  if (animation == "chomper") {
    drawDotChomper(now);
    nextAnimationFrame = now + 35;
  } else if (animation == "pup") {
    drawWalkingPup(now);
    nextAnimationFrame = now + 60;
  } else if (isGifAnimation(animation)) {
    int frameDelay = 100;
    if (!gif.playFrame(false, &frameDelay)) {
      gif.reset();
      if (!gif.playFrame(false, &frameDelay)) {
        Serial.println("GIF playback failed; returning to text screen.");
        animation = "text";
        activeAnimation = "";
        return;
      }
    }
    nextAnimationFrame = now + max(frameDelay, 20);
  }
}
} // namespace

void setup() {
  Serial.begin(115200);
  pinMode(TFT_POWER, OUTPUT);
  digitalWrite(TFT_POWER, HIGH);
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  if (!display->begin()) {
    Serial.println("Display initialization failed.");
    while (true) {
      delay(1000);
    }
  }

  fileSystemReady = LittleFS.begin(true);
  if (!fileSystemReady) {
    Serial.println("LittleFS is unavailable; GIF uploads are disabled.");
  }

  if (!loadSettings()) {
    Serial.println("Could not open settings storage.");
    while (true) {
      delay(1000);
    }
  }
  renderTextScreen();

  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Could not start the Wi-Fi access point.");
    while (true) {
      delay(1000);
    }
  }

  server.on("/", HTTP_GET, []() { server.send(200, "text/html", makePage()); });
  server.on("/save", HTTP_POST, handleSave);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadData);
  server.on("/delete-gif", HTTP_POST, handleDeleteGif);
  server.begin();

  Serial.print("Connect to Wi-Fi: ");
  Serial.println(AP_SSID);
  Serial.print("Open http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  updateAnimation(millis());
}
