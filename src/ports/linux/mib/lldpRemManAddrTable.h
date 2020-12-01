/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2020 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

/*
 * Note: this file originally auto-generated by mib2c
 * using mib2c.iterate.conf
 */
#ifndef LLDPREMMANADDRTABLE_H
#define LLDPREMMANADDRTABLE_H

#include "pf_includes.h"

/* function declarations */
void init_lldpRemManAddrTable (pnet_t * pnet);
void initialize_table_lldpRemManAddrTable (pnet_t * pnet);
Netsnmp_Node_Handler lldpRemManAddrTable_handler;
Netsnmp_First_Data_Point lldpRemManAddrTable_get_first_data_point;
Netsnmp_Next_Data_Point lldpRemManAddrTable_get_next_data_point;

/* column number definitions for table lldpRemManAddrTable */
#define COLUMN_LLDPREMMANADDRSUBTYPE   1
#define COLUMN_LLDPREMMANADDR          2
#define COLUMN_LLDPREMMANADDRIFSUBTYPE 3
#define COLUMN_LLDPREMMANADDRIFID      4
#define COLUMN_LLDPREMMANADDROID       5
#endif /* LLDPREMMANADDRTABLE_H */