#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> // 쓰레드 사용을 위해 추가

// 핀 번호 (BCM)
#define LED_RED_PIN 12
#define LED_GREEN_PIN 13
#define CONSUMER "Network_Monitor"

struct gpiod_chip *chip;
struct gpiod_line *line_red;
struct gpiod_line *line_green;

// PWM 제어를 위한 변수
volatile int target_brightness = 0; // 0 ~ 100 (붉은색 LED 밝기)
volatile int running = 1;           // 프로그램 실행 상태
pthread_t pwm_tid;                  // 쓰레드 ID

// -------------------------------
// 소프트웨어 PWM 쓰레드 함수
// -------------------------------
void *pwm_thread_func(void *arg) {
    (void)arg;
    // 주기: 10ms (100Hz) - 눈의 잔상 효과 이용
    // 100단계 밝기 조절을 위해 1틱당 100us (10ms / 100)
    
    while (running) {
        int brightness = target_brightness;

        if (brightness <= 0) {
            if (line_red) gpiod_line_set_value(line_red, 0);
            usleep(10000); // 10ms 대기
        } 
        else if (brightness >= 100) {
            if (line_red) gpiod_line_set_value(line_red, 1);
            usleep(10000);
        } 
        else {
            // PWM 사이클 생성 (ON 구간 + OFF 구간)
            if (line_red) gpiod_line_set_value(line_red, 1);
            usleep(brightness * 100); // ON 시간 (예: 30 * 100us = 3ms)

            if (line_red) gpiod_line_set_value(line_red, 0);
            usleep((100 - brightness) * 100); // OFF 시간 (예: 70 * 100us = 7ms)
        }
    }
    return NULL;
}

// -------------------------------
// GPIO 초기화
// -------------------------------
int init_gpio(void) {
    chip = gpiod_chip_open_by_number(0);
    if (!chip) return -1;

    line_red = gpiod_chip_get_line(chip, LED_RED_PIN);
    line_green = gpiod_chip_get_line(chip, LED_GREEN_PIN);

    if (!line_red || !line_green) {
        gpiod_chip_close(chip);
        return -1;
    }

    gpiod_line_request_output(line_red, CONSUMER, 0);
    gpiod_line_request_output(line_green, CONSUMER, 0);

    // PWM 쓰레드 시작
    if (pthread_create(&pwm_tid, NULL, pwm_thread_func, NULL) != 0) {
        perror("Thread creation failed");
        return -1;
    }

    return 0;
}

// -------------------------------
// LED 제어 함수들 (PWM 지원)
// -------------------------------

// 밝기 설정 (0 ~ 100)
void set_red_brightness(int brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    target_brightness = brightness;
}

void show_normal(void) {
    // 정상: 녹색 켜짐, 빨강은 은은하게 (밝기 10%)
    if (line_green) gpiod_line_set_value(line_green, 1);
    set_red_brightness(10); 
}

void show_eth0_error(void) {
    // 에러: 녹색 끔, 빨강 3번 밝게 깜빡임
    if (line_green) gpiod_line_set_value(line_green, 0);
    
    // 깜빡임 효과 (밝기 10 <-> 0)
    for(int i=0; i<3; i++) {
        set_red_brightness(10);
        usleep(300000); // 300ms
        set_red_brightness(0);
        usleep(300000);
    }
}

void show_network_error(void) {
    // 에러: 녹색 끔, 빨강 2번 깜빡임 
    if (line_green) gpiod_line_set_value(line_green, 0);
    
    // 깜빡임 효과 (밝기 10 <-> 0)
    for(int i=0; i<2; i++) {
        set_red_brightness(10);
        usleep(300000); // 300ms
        set_red_brightness(0);
        usleep(300000);
    }
}

// -------------------------------
// 종료 및 유틸리티
// -------------------------------
void handle_sigint(int sig) {
    (void)sig;
    printf("\n[INFO] Cleanup...\n");
    running = 0; // 쓰레드 종료 신호
    pthread_join(pwm_tid, NULL); // 쓰레드 종료 대기

    if (line_red) gpiod_line_set_value(line_red, 0);
    if (line_green) gpiod_line_set_value(line_green, 0);
    
    gpiod_line_release(line_red);
    gpiod_line_release(line_green);
    gpiod_chip_close(chip);
    _exit(0);
}

int check_eth0_link(void) {
    FILE *fp = fopen("/sys/class/net/eth0/carrier", "r");
    if (!fp) return 0;
    int link = 0;
    fscanf(fp, "%d", &link);
    fclose(fp);
    return link == 1;
}

int check_internet_reachability(void){
    // 구글 DNS(8.8.8.8)로 Ping을 1회 보냄
    // -c 1: 1번만 보냄
    // -W 2: 2초 안에 응답 없으면 실패로 간주 (너무 오래 걸리면 안 되니까)
    // > /dev/null: 화면에 출력되는 잡다한 글씨는 버림
    int status = system("ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1");
    return (status == 0);
}

// -------------------------------
// Main
// -------------------------------
int main(void) {
    signal(SIGINT, handle_sigint);
    setvbuf(stdout, NULL, _IONBF, 0);

    if (init_gpio() < 0) {
        fprintf(stderr, "GPIO Init Failed\n");
        return 1;
    }

    printf("[INFO] Network Monitor Started (Soft PWM Mode)\n");

    while (1) {
        if(check_eth0_link()){
            if(check_internet_reachability()){
                show_normal();
            }else{
                printf("[ERR] Network down\n");
                show_network_error();
            }
        }else{
            printf("[ERR] eth0 link down\n");
            show_eth0_error();
        }
        sleep(5);
    }
    return 0;
}
