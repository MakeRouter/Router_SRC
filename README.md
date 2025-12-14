# Router_SRC v1.1.0
- 라즈비안 OS 에서 OpenWrt OS로 포팅한 내용 ( 서비스는 OpenWrt OS로 변경하면서 자동 관리로 인해 없앰 )
- LED 2개(GREEN/RED)를 이용해 시스템 상태를 시각적으로 표시함

---

## 수정된 부분
- wiringpi.h -> gpiod.h

---

## 정상 동작 시 LED 상태 요약

| RED     | GREEN   | 의미                      |
| ------- | ------- | ----------------------- |
| X     | X     | 전원 꺼짐                   |
| 3회 점등   | X     | WAN선 연결 오류      |
| 2회 점등 | X | 네트워크 연결 오류 ( 인터넷 연결 안됨 ) |
| O      | O      | 정상 연결                   |

---

## 프로젝트 경로 및 실행 파일
- /home/pi/Router 경로를 기준

```
cd /root
mkdir Network_service
cd /root/Network_service
```

---

## main.c 코드 빌드 ( 크로스 컴파일 ) 

- 크로스 컴파일 귀찮으면 위에 올려둔 LEDControl 실행파일 가져다 쓰면 됩니다.

```
make
```
---

## 서비스 등록

- 부팅 시 자동 실행되도록 systemd 서비스 등록을 진행함함



