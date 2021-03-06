// ==============================================================
// File generated by Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC
// Version: 2017.2
// Copyright (C) 1986-2017 Xilinx, Inc. All Rights Reserved.
// 
// ==============================================================

// AXILiteS
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - Channel 0 (ap_done)
//        bit 1  - Channel 1 (ap_ready)
//        others - reserved
// 0x10 : Data signal of width_V
//        bit 11~0 - width_V[11:0] (Read/Write)
//        others   - reserved
// 0x14 : reserved
// 0x18 : Data signal of height_V
//        bit 11~0 - height_V[11:0] (Read/Write)
//        others   - reserved
// 0x1c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XSTPG_AXILITES_ADDR_AP_CTRL       0x00
#define XSTPG_AXILITES_ADDR_GIE           0x04
#define XSTPG_AXILITES_ADDR_IER           0x08
#define XSTPG_AXILITES_ADDR_ISR           0x0c
#define XSTPG_AXILITES_ADDR_WIDTH_V_DATA  0x10
#define XSTPG_AXILITES_BITS_WIDTH_V_DATA  12
#define XSTPG_AXILITES_ADDR_HEIGHT_V_DATA 0x18
#define XSTPG_AXILITES_BITS_HEIGHT_V_DATA 12

