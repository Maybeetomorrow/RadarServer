#!/bin/bash

# Расшифровываем SSID и пароль
echo "Расшифровка данных..."
SSID=$(openssl enc -d -aes-256-cbc -a -salt -pbkdf2 -iter 10000 -pass file:/etc/wifi_ap/encryption_key -in /etc/wifi_ap/encrypted_ssid)
PASSWORD=$(openssl enc -d -aes-256-cbc -a -salt -pbkdf2 -iter 10000 -pass file:/etc/wifi_ap/encryption_key -in /etc/wifi_ap/encrypted_password)

# Создание директории для резервного копирования
mkdir -p /etc/backup/

# Копирование исходных файлов в резерв
cp -f /etc/hostapd/hostapd.conf /etc/backup/hostapd.conf.original 2>/dev null || true
cp -f /etc/dnsmasq.conf /etc/backup/dnsmasq.conf.original 2>/dev null || true

# Сохранение состояния включения сервисов
echo "NetworkManager: $(systemctl is-enabled NetworkManager)" > /etc/backup/enabled_states.txt
echo "wpa_supplicant: $(systemctl is-enabled wpa_supplicant)" >> /etc/backup/enabled_states.txt
echo "systemd-resolved: $(systemctl is-enabled systemd-resolved)" >> /etc/backup/enabled_states.txt

# Сохранение состояния работы сервисов
echo "NetworkManager: $(systemctl is-active NetworkManager)" > /etc/backup/running_states.txt
echo "wpa_supplicant: $(systemctl is-active wpa_supplicant)" >> /etc/backup/running_states.txt
echo "systemd-resolved: $(systemctl is-active systemd-resolved)" >> /etc/backup/running_states.txt

# Сохранение настройки IP-пересылки
sysctl net.ipv4.ip_forward | grep -oP '(?<= ).*' > /etc/backup/ip_forward.txt

# Сохранение правил iptables
iptables-save > /etc/backup/iptables.original

# Выполнение изменений
# Остановка и отключение сервисов
systemctl stop NetworkManager
systemctl stop wpa_supplicant
systemctl stop systemd-resolved
systemctl disable NetworkManager
systemctl disable wpa_supplicant
systemctl disable systemd-resolved

# Перезапуск dnsmasq
systemctl restart dnsmasq

# Добавление интерфейса ap0
iw dev wlp0s20f3 interface add ap0 type __ap

# Настройка IP-адреса для ap0
ip addr add 192.168.1.1/24 dev ap0

# Включение интерфейса ap0
ip link set dev ap0 up

# Отключение IP-пересылки
sysctl -w net.ipv4.ip_forward=0

# Очистка правил iptables
iptables -F

# Настройка конфигурационного файла hostapd
echo -e "interface=ap0\ndriver=nl80211\nssid=$SSID\nhw_mode=g\nchannel=6\nwpa=2\nwpa_passphrase=$PASSWORD\nwpa_key_mgmt=WPA-PSK\nrsn_pairwise=CCMP" > /etc/hostapd/hostapd.conf

# Изменение /etc/default/hostapd
sed -i 's/#DAEMON_CONFF=""/DAEMON_CONFF="\/etc\/hostapd\/hostapd.conf"/g' /etc/default/hostapd

# Настройка конфигурационного файла dnsmasq
echo -e "interface=ap0\ndhcp-range=192.168.1.50,192.168.1.150,255.255.255.0,24h\ndhcp-option=3,192.168.1.1\nno-resolv\nbogus-priv" > /etc/dnsmasq.conf

# Перезапуск dnsmasq
systemctl restart dnsmasq

# Разблокировка и запуск hostapd
systemctl unmask hostapd
systemctl start hostapd
