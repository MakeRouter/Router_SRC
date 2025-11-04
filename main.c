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
    pinMode(RED, OUTPUT);
    pinMode(GREEN, OUTPUT);
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
}

void led_off_all(void) {
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
}

void blink_led(int pin, int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        digitalWrite(pin, HIGH);
        delay(delay_ms);
        digitalWrite(pin, LOW);
        delay(delay_ms);
    }
}

void blink_led_both(int pin_1 , int pin_2, int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        digitalWrite(pin_1, HIGH);
        digitalWrite(pin_2, HIGH);
        delay(delay_ms);
        digitalWrite(pin_1, LOW);
        digitalWrite(pin_2, LOW);
        delay(delay_ms);
    }
}

// -------------------------------
// LED 상태 표시
// -------------------------------
void show_normal(void) {
    digitalWrite(RED, HIGH);
    digitalWrite(GREEN, HIGH);
}

void show_network_error(void) {
    digitalWrite(GREEN, LOW);
    blink_led(RED, 4, 300);
}

void show_code_running(void) {
    digitalWrite(RED, LOW);
    blink_led(GREEN, 4, 300);
}

void service_error(void){
    blink_led_both(RED, GREEN, 4, 300);
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
        if (recover_network_services()) {
            show_normal();
        } else {
            show_network_error();
            printf("[FATAL] Some services failed. System rebooting...\n");
            //system("sudo reboot");
        }
        delay(10000); // 10초마다 감시
    }

    return 0;
}
