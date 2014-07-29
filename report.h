/*
 * =====================================================================================
 *
 *       Filename:  report.h
 *
 *    Description:  汇报服务器状态信息
 *
 *        Version:  1.0
 *        Created:  03/01/2013 01:59:22 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  聂汉子 (niehanzi), niehanzi@qiyi.com
 *        Company:  奇艺世纪
 *
 * =====================================================================================
 */
#ifndef _REPORT_H_
#define _REPORT_H_
#include <utils/utils.h>
#include <curl/curl.h>
#include "config.h"
#include "sw_scaner.h"
#include "topology.h"

int report_do(topology_t *topology, sw_scaner_t *sw_scaner, int timeout);

#endif
