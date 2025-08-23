#pragma once

#define LR  0
#define SP  1
#define CS  2
#define RS  3
#define RB  4
#define DP  5
#define VA  6
#define LA  7
#define RC  8
#define N1  9
#define S2 10
#define S1 11
#define N4 12
#define DS 13
#define DT 14
#define S0 15

#define DSO 4
#define RSO 8

#define STACK_DATA            0
#define STACK_LEN           ( 2 * 4)
#define RUNTIME_CBS         ( 3 * 4)
#define SPECIAL_RUNTIME_CBS ( 4 * 4)
#define MEMORY              ( 5 * 4)
#define DATA                ( 6 * 4)
#define CALLBACK_INFO       ( 7 * 4)
#define CALLBACK_LOCATIONS  ( 8 * 4)
#define DEPTH               ( 9 * 4)
#define VARIABLES           (10 * 4)
#define CALLBACK_TARGETS    (11 * 4)
#define LITERALS            (12 * 4)
#define RETURN_STACK_DATA   (13 * 4)
#define BIN_START           (14 * 4)

#ifdef M4_ESP32S3_DEFINE_REG_ALIASES
#define ACAT_(n) a##n
#define ACAT(n) ACAT_(n)

#define lr ACAT(LR)
#define sp ACAT(SP)
#define cs ACAT(CS)
#define rs ACAT(RS)
#define rb ACAT(RB)
#define dp ACAT(DP)
#define va ACAT(VA)
#define la ACAT(LA)
#define rc ACAT(RC)
#define n1 ACAT(N1)
#define s2 ACAT(S2)
#define s1 ACAT(S1)
#define n4 ACAT(N4)
#define ds ACAT(DS)
#define dt ACAT(DT)
#define s0 ACAT(S0)

#define dso DSO
#define rso RSO

#endif /*M4_ESP32S3_DEFINE_REG_ALIASES*/
