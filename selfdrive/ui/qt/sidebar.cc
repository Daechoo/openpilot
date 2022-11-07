#include "selfdrive/ui/qt/sidebar.h"

#include <QMouseEvent>

#include "selfdrive/common/util.h"
#include "selfdrive/hardware/hw.h"
#include "selfdrive/ui/qt/util.h"

void Sidebar::drawMetric(QPainter &p, const QString &label, QColor c, int y) {
  const QRect rect = {30, y, 240, label.contains("\n") ? 140 : 140};

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(c));
  p.setClipRect(rect.x() + 6, rect.y(), 18, rect.height(), Qt::ClipOperation::ReplaceClip);
  p.drawRoundedRect(QRect(rect.x() + 6, rect.y() + 6, 100, rect.height() - 12), 10, 10);
  p.setClipping(false);

  QPen pen = QPen(QColor(0xff, 0xff, 0xff, 0x55));
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(rect, 20, 20);

  p.setPen(QColor(0xff, 0xff, 0xff));
  configFont(p, "Open Sans", 35, "Regular");
  const QRect r = QRect(rect.x() + 35, rect.y(), rect.width() - 50, rect.height());
  p.drawText(r, Qt::AlignCenter, label);
}

Sidebar::Sidebar(QWidget *parent) : QFrame(parent) {
  home_img = QImage("../assets/images/button_home.png").scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  settings_img = QImage("../assets/images/button_settings.png").scaled(settings_btn.width(), settings_btn.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

  connect(this, &Sidebar::valueChanged, [=] { update(); });

  setAttribute(Qt::WA_OpaquePaintEvent);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  setFixedWidth(300);
}

void Sidebar::mouseReleaseEvent(QMouseEvent *event) {
  if (settings_btn.contains(event->pos())) {
    emit openSettings();
  }
}

void Sidebar::updateState(const UIState &s) {
  auto &sm = *(s.sm);

  auto deviceState = sm["deviceState"].getDeviceState();
  setProperty("netType", network_type[deviceState.getNetworkType()]);
  int strength = (int)deviceState.getNetworkStrength();
  setProperty("netStrength", strength > 0 ? strength + 1 : 0);
  setProperty("wifiAddr", deviceState.getWifiIpAddress().cStr());

  ItemStatus connectStatus;
  auto last_ping = deviceState.getLastAthenaPingTime();
  if (last_ping == 0) {
    connectStatus = params.getBool("PrimeRedirected") ? ItemStatus{"NO\nPRIME", danger_color} : ItemStatus{"CONNECT\nOFFLINE", warning_color};
  } else {
    connectStatus = nanos_since_boot() - last_ping < 80e9 ? ItemStatus{"CONNECT\nONLINE", good_color} : ItemStatus{"CONNECT\nERROR", danger_color};
  }
  setProperty("connectStatus", QVariant::fromValue(connectStatus));

  m_ambientTemp = deviceState.getAmbientTempC();

  for (auto const & t : deviceState.getCpuTempC()) {
    if (t > m_ambientTemp) {
      m_ambientTemp = t;
    }
  }
  for (auto const & t : deviceState.getGpuTempC()) {
    if (t > m_ambientTemp) {
      m_ambientTemp = t;
    }
  }
  char val[1024];
  ItemStatus tempStatus;

  auto ts = deviceState.getThermalStatus();
  if (ts == cereal::DeviceState::ThermalStatus::GREEN) {
    snprintf(val, sizeof(val), "%.1f%s\nGOOD\nCPU", m_ambientTemp, "°C");
    tempStatus = {val, good_color};
  } else if (ts == cereal::DeviceState::ThermalStatus::YELLOW) {
    snprintf(val, sizeof(val), "%.1f%s\nOK\nCPU", m_ambientTemp, "°C");
    tempStatus = {val, warning_color};
  }
  else {
    snprintf(val, sizeof(val), "%.1f%s\nHIGH_TEMP", m_ambientTemp, "°C");
    tempStatus = {val, danger_color};
  }
  setProperty("tempStatus", QVariant::fromValue(tempStatus));

  ItemStatus pandaStatus = {"VEHICLE\nONLINE", good_color};
  if (s.scene.pandaType == cereal::PandaState::PandaType::UNKNOWN) {
    pandaStatus = {"NO\nPANDA", danger_color};
  } else if (s.scene.started && !sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK()) {
    pandaStatus = {"GPS\nSEARCHING", warning_color};
  }
  setProperty("pandaStatus", QVariant::fromValue(pandaStatus));
  m_battery_img = deviceState.getBatteryStatus() == "Charging" ? 1 : 0;
  m_batteryPercent = deviceState.getBatteryPercent();
}

void Sidebar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(Qt::NoPen);
  p.setRenderHint(QPainter::Antialiasing);

  p.fillRect(rect(), QColor(57, 57, 57));

  // static imgs
  p.setOpacity(0.65);
  p.drawImage(settings_btn.x(), settings_btn.y(), settings_img);
  p.setOpacity(1.0);
  p.drawImage(60, 1080 - 180 - 40, home_img);

  // network
  int x = 58;
  const QColor gray(0x54, 0x54, 0x54);
  for (int i = 0; i < 5; ++i) {
    p.setBrush(i < net_strength ? Qt::white : gray);
    p.drawEllipse(x, 196, 27, 27);
    x += 37;
  }

  configFont(p, "Open Sans", 32, "Regular");
  p.setPen(QColor(0xff, 0xff, 0xff));
  const QRect r = QRect(20, 230, 250, 50);
  if(Hardware::EON() && net_type == network_type[cereal::DeviceState::NetworkType::WIFI])
    p.drawText(r, Qt::AlignCenter, wifi_addr);
  else
    p.drawText(r, Qt::AlignCenter, net_type);

  //battery 
  QRect  rect(45, 293, 96, 36);
  QRect  bq(50, 298, int(76* m_batteryPercent * 0.01), 25);
  QBrush bgBrush("#149948");
  p.fillRect(bq,  bgBrush);
  p.drawImage(rect, battery_imgs[m_battery_img]);

  p.setPen(Qt::white);
  configFont(p, "Open Sans", 30, "Regular");

  char battery_str[32];
  //char temp_str[16];

  const QRect bt = QRect(170, 288, event->rect().width(), 50);
  snprintf(battery_str, sizeof(battery_str), "%d%%", m_batteryPercent);
  p.drawText(bt, Qt::AlignLeft, battery_str);
  // ambient Temp
  //const QRect temp = QRect(40, 367, 240, 50);
  //snprintf(temp_str, sizeof(temp_str), "%.1f%s", m_ambientTemp, "°C");
  //p.drawText(temp, Qt::AlignCenter, temp_str);
  // metrics
  drawMetric(p, temp_status.first, temp_status.second, 345);
  drawMetric(p, panda_status.first, panda_status.second, 505);
  drawMetric(p, connect_status.first, connect_status.second, 665);
}
