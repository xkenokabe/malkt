﻿// Copyright(c) 2020 Ken Okabe
// This software is released under the MIT License, see LICENSE.
#include "uptime.h"

static uint64_t the_uptime = 0;

static uint64_t Get(void) { return the_uptime; }

static void Set(uint64_t milliseconds) { the_uptime = milliseconds; }

static const UptimeMethodStruct kTheMethod = {
    .Get = Get,
    .Set = Set,
};

const UptimeMethod uptime = &kTheMethod;