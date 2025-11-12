#include <wiringPi.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define RED   13
#define GREEN 12

// -------------------------------
// 공통 유틸리티
// -------------------------------
void init_gpio(void) {
    if (wiringPiSetupGpio() == -1) {
        printf("[ERR] wiringPi setup failed\n");
        _exit(1);
    }
    pinMode(RED, PWM_OUTPUT);
    pinMode(GREEN, OUTPUT);

    // PWM 설정
    pwmSetMode(PWM_MODE_MS);        //마크 스페이스 모드 -> 정확한 주기
    pwmSetRange(1024);              //PWM 값 범위 0 ~ 1023
    pwmSetClock(192);               //PWM 주파수 조정 ( 약 1000Hz 근처 )

    pwmWrite(RED, 0);
    digitalWrite(GREEN, LOW);
}

void set_led_brightness(int pin, int brightness){
    if(brightness < 0) brightness = 0;
    if(brightness > 1023) brightness = 1023;
    pwmWrite(pin, brightness);
}

void led_off_all(void) {
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
}

void blink_led(int pin, int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        digitalWrite(pin, LOW);
        delay(delay_ms);
        digitalWrite(pin, HIGH);
        delay(delay_ms);
    }
}

void blink_led_both(int pin_1 , int pin_2, int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        pwmWrite(pin_1, 0);
        digitalWrite(pin_2, LOW);
        delay(delay_ms);
        pwmWrite(pin_1, 100);
        digitalWrite(pin_2, HIGH);
        delay(delay_ms);
    }
}

// -------------------------------
// LED 상태 표시
// -------------------------------
void show_normal(void) {
    //digitalWrite(RED, HIGH);
    set_led_brightness(RED, 100);
    digitalWrite(GREEN, HIGH);
}

void show_network_error(void) {
    digitalWrite(GREEN, LOW);
    blink_led(RED, 3, 300);
}

void show_code_running(void) {
    digitalWrite(RED, LOW);
    blink_led(GREEN, 3, 300);
}

void service_error(void){
    blink_led_both(RED, GREEN, 3, 300);
}

// -------------------------------
// SIGINT 핸들러
// -------------------------------
void handle_sigint(int sig) {
    printf("\n[INFO] System off - LEDs off\n");
    led_off_all();
    pinMode(RED, INPUT);
    pinMode(GREEN, INPUT);
    _exit(0);
}

// -------------------------------
// 서비스 상태 관리
// -------------------------------
int check_service_status(const char *service) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "systemctl is-active %s > /tmp/svc_status 2>/dev/null", service);
    system(cmd);

    FILE *fp = fopen("/tmp/svc_status", "r");
    if (!fp) return 0;

    char status[64];
    fgets(status, sizeof(status), fp);
    fclose(fp);

    return (strncmp(status, "active", 6) == 0);
}

void restart_service(const char *service) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "sudo systemctl restart %s", service);
    system(cmd);
    delay(2000); // 2초 대기 후 재확인
}

// -------------------------------
// eth0 링크 상태 확인 함수 추가
// -------------------------------
int check_eth0_link(void) {
    FILE *fp = fopen("/sys/class/net/eth0/carrier", "r");
    if (!fp) {
        perror("[ERR] eth0 carrier check failed");
        return 0;
    }
    int link = 0;
    fscanf(fp, "%d", &link);
    fclose(fp);
    return link == 1;  // 1이면 연결됨, 0이면 연결 안됨
}

// -------------------------------
// 네트워크 복구 순서
// -------------------------------
int recover_network_services(void) {
    const char *services[] = {"dhcpcd.service", "hostapd.service", "dnsmasq.service"};
    int ok = 1;

    for (int i = 0; i < 3; i++) {
        const char *svc = services[i];
        if (!check_service_status(svc)) {
            printf("[WARN] %s inactive → restarting...\n", svc);
            service_error(); // 진행 중 표시
            restart_service(svc);
            if (!check_service_status(svc)) {
                printf("[ERR] %s restart failed!\n", svc);
                ok = 0;
            } else {
                printf("[OK] %s running\n", svc);
            }
        } else {
            printf("[OK] %s active\n", svc);
        }
    }
    return ok;
}

// -------------------------------
// main()
// -------------------------------
int main(void) {
    signal(SIGINT, handle_sigint);
    setvbuf(stdout, NULL, _IONBF, 0);
    init_gpio();

    printf("[INFO] Network monitor started\n");

    while (1) {
        int service_ok = recover_network_services();
        int eth_link_ok = check_eth0_link();

        if (service_ok && eth_link_ok) {
            show_normal();
        } else {

            if (!eth_link_ok)
                show_network_error();
                printf("[ERR] eth0 link down (케이블 미연결 or 인터넷 끊김)\n");
            if (!service_ok)
                printf("[ERR] One or more network services failed\n");

            printf("[FATAL] Some services or link failed. System rebooting...\n");
            led_off_all();
            //system("sudo reboot");
        }

        delay(10000); // 10초마다 감시
    }

    return 0;
}
