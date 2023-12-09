/*
 * CmdFormat Command format definition
 * Copyright (c) 2016 Shenghua Su
 *
 *
 */

#ifndef _CMD_FORMAT_H_INCLUDED
#define _CMD_FORMAT_H_INCLUDED

// ----------------- cmd data content ------------------
// command data content formate:
//              ++----------------+-------------------------------------++
//  byte No.:   ||     0 ~ 1      |          2 ~ size - 1               ||
//              ++----------------+-------------------------------------++
//  byte name:  ||    cmd key     |            cmd args                 ||
//              ++----------------+-------------------------------------++
//
//  Note: data length must >= 2, byte 0 and 1 depicted as cmd key,
//        optionally bytes (2 to size - 1) represent command args

#define CMD_DATA_KEY_SIZE            2
#define CMD_DATA_AT_LEAST_SIZE       2

#define CMD_DATA_KEY_OFFSET          0
#define CMD_DATA_ARG_OFFSET          2

// ----------------- cmd return data ------------------
// command return data formate:
//              ++----------------+-------------------------------------++
//  byte No.:   ||       0        |          1 ~ retsize - 1            ||
//              ++----------------+-------------------------------------++
//  byte name:  ||  status code   |            return data              ||
//              ++----------------+-------------------------------------++
//
//  Note: command return data is optional, command may or may not return
//        them to request-made side

#define CMD_RET_DATA_STATUS_CODE_SIZE       1
#define CMD_RET_DATA_AT_LEAST_SIZE          1

#define CMD_RET_DATA_STATUS_CODE_OFFSET     0
#define CMD_RET_DATA_CONTENT_OFFSET         1

#endif // _CMD_FORMAT_H_INCLUDED
