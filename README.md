# Router_SRC v1.0.1
- 라즈베리파이 기반 라우터 환경에서 dhcpcd, hostapd, dnsmasq 서비스 상태를 주기적으로 점검하고 비정상 상태일 경우 자동 복구 및 재부팅을 수행하는 네트워크 모니터링 프로그램
- LED 2개(GREEN/RED)를 이용해 시스템 상태를 시각적으로 표시함

---

## 수정된 오류
- RED LED의 3회 점등 이슈 -> PWM 제어시 일반 GPIO 제어로 하여 발생한 이슈 해결


---

## 정상 동작 시 LED 상태 요약

| RED     | GREEN   | 의미                      |
| ------- | ------- | ----------------------- |
| X     | X     | 전원 꺼짐                   |
| O      | X     | 전원 켜짐 (대기)              |
| X     | O   | 코드 실행 중                 |
| 3회 점등   | X     | 인터넷 연결 오류      |
| O      | O      | 정상 연결                   |
| 3회 점등 | 3회 점등 | 네트워크 서비스 오류, 복구 시도 |

---

## 주요 서비스 복구 순서

- 프로그램은 10초마다 다음 순서로 서비스 상태를 점검함

1. dhcpcd.service
2. hostapd.service
3. dnsmasq.service

- 각 서비스가 비활성 상태(inactive) 이거나 실패 상태(failed) 일 경우 자동으로 재시작함함

---

## wiringPi 설치

```
sudo apt update
sudo apt install -y git
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build
gpio -v
```
---

## 프로젝트 경로 및 실행 파일
- /home/pi/Router 경로를 기준

```
cd /home/pi
mkdir Router
cd Router
```

---

## main.c 코드 빌드

```
gcc main.c -o main -lwiringPi
```
---

## systemd 서비스 등록

- 부팅 시 자동 실행되도록 systemd 서비스 등록을 진행함함

### systemd 서비스 등록

```
sudo nano /etc/systemd/system/router-monitor.service

[Unit]
Description=Router Network Monitor and Auto Recovery
After=network.target multi-user.target

[Service]
Type=simple
WorkingDirectory=/home/pi/Router
ExecStart=/usr/bin/sudo /home/pi/Router/main
Restart=always
RestartSec=5
User=pi
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### 서비스 등록 및 활성화

```
sudo chmod 644 /etc/systemd/system/router-monitor.service
sudo systemctl daemon-reload
sudo systemctl enable router-monitor.service
sudo systemctl start router-monitor.service
```


