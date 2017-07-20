/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

////////////////////////////////////////
// Harness interface include files
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

////////////////////////////////////////
// Test system include files
#include "test/helpers.h"

////////////////////////////////////////
// Third-party include files
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <iostream>
#include <fstream>
#include <climits>

using std::cout;
using std::endl;

using mysql_harness::Loader;
using mysql_harness::Path;

Path g_here;

class KeepalivePluginTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Path here = Path(g_here);
    orig_cout_ = std::cout.rdbuf();
    std::cout.rdbuf(ssout.rdbuf());

    std::map<std::string, std::string> params;
    params["program"] = "harness";
    params["prefix"] = here.c_str();
    params["log_level"] = "info";

    config_.reset(new mysql_harness::LoaderConfig(params, std::vector<std::string>(),
                                                  mysql_harness::Config::allow_keys));
    config_->read(here.join("data/keepalive.cfg"));
    loader = new Loader("harness", *config_);
  }

  virtual void TearDown() {
    std::cout.rdbuf(orig_cout_);
    delete loader;
    loader = nullptr;
  }

  Loader *loader;
  std::unique_ptr<mysql_harness::LoaderConfig> config_;

 private:
  std::stringstream ssout;
  std::streambuf *orig_cout_;
};

TEST_F(KeepalivePluginTest, Available) {
  auto lst = loader->available();
  EXPECT_EQ(1U, lst.size());

  EXPECT_SECTION_AVAILABLE("keepalive", loader);
}

TEST_F(KeepalivePluginTest, CheckLog) {
  auto logging_folder = g_here.join("/var/log/keepalive");
  const auto log_file = Path::make_path(logging_folder, "harness", "log");
  init_test_logger({"keepalive"}, loader->get_config().get_default("logging_folder"), "harness");

  // Make sure log file is empty
  std::fstream fs;
  fs.open(log_file.str(), std::fstream::trunc | std::ofstream::out);
  fs.close();

  ASSERT_NO_THROW(loader->start());

  std::ifstream ifs_log(log_file.str());
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(ifs_log, line)) {
    lines.push_back(line);
  }

  auto find_line = [&lines](unsigned start_line, const char* needle) -> unsigned {
    for (unsigned i = start_line; i < lines.size(); i++)
      if (lines[i].find(needle) != std::string::npos)
        return i;
    return UINT_MAX;
  };

  ASSERT_GE(lines.size(), 4U);
  unsigned start_line = 0;
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "keepalive started with interval 1"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "2 time(s)"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "keepalive"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "INFO"));
  EXPECT_NE(UINT_MAX, start_line = find_line(start_line, "keepalive"));
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname().str();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
