//
//
//  test_app.h
//  QHWI
//
//  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
//  SPDX-License-Identifier: BSD-3-Clause-Clear
//
#pragma once

#include "unit_test.h"
#include <cxxopts.hpp>

class TestApp
{
public:
    TestApp(int argc, const char* const* argv);
    std::vector<std::unique_ptr<UnitTest>> tests;
    std::string gtestOutputPath;

private:
    cxxopts::ParseResult params;
    std::vector<std::string> testsParam;
    std::vector<std::string> excludeTestsParam;
    std::vector<std::string> backends;
    UnitTest::Config config;
};