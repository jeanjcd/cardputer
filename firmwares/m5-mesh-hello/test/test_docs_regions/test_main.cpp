// SPDX-License-Identifier: GPL-3.0-or-later
// Checks that the region table in docs/en/frequencies.md (between the markers
// <!-- regions:begin --> and <!-- regions:end -->) matches src/mesh/region.*:
// same regions in the same order, band edges, LongFast frequency, power limit,
// duty cycle and experimental flag.
#include <unity.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "mesh/region.h"

void setUp() {}
void tearDown() {}

namespace {

// Reads the whole file, trying the project-relative path first (pio runs tests from the project folder),
// then a path derived from this source file's location.
bool readDoc(std::string& out) {
  std::vector<std::string> paths = {"docs/en/frequencies.md"};
  std::string self = __FILE__;
  size_t cut = self.find_last_of("/\\");
  if (cut != std::string::npos) paths.push_back(self.substr(0, cut) + "/../../docs/en/frequencies.md");
  for (const std::string& p : paths) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) continue;
    char buf[4096];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
  }
  return false;
}

std::string trim(const std::string& s) {
  const char* ws = " \t\r*`";
  size_t a = s.find_first_not_of(ws);
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(ws);
  return s.substr(a, b - a + 1);
}

std::vector<std::string> cells(const std::string& line) {
  std::vector<std::string> out;
  size_t start = line.find('|');
  while (start != std::string::npos) {
    size_t next = line.find('|', start + 1);
    if (next == std::string::npos) break;
    out.push_back(trim(line.substr(start + 1, next - start - 1)));
    start = next;
  }
  return out;
}

// Parses the first number in s (skipping non-numeric characters); false if none.
bool firstNumber(const char*& s, double& v) {
  while (*s && !((*s >= '0' && *s <= '9') || *s == '.')) s++;
  if (!*s) return false;
  char* end = nullptr;
  v = strtod(s, &end);
  if (end == s) return false;
  s = end;
  return true;
}

struct Row {
  std::string name;
  double bandLo, bandHi, freq, power, duty;
  bool experimental;
};

// Table columns: Region | Countries | Band (MHz) | LongFast (MHz) | Max power (dBm) | Duty cycle (%) | Experimental
std::vector<Row> parseTable(const std::string& doc) {
  std::vector<Row> rows;
  size_t b = doc.find("<!-- regions:begin -->");
  size_t e = doc.find("<!-- regions:end -->");
  TEST_ASSERT_TRUE_MESSAGE(b != std::string::npos && e != std::string::npos && b < e, "region markers missing");
  std::string body = doc.substr(b, e - b);
  size_t pos = 0;
  bool header = true;
  while (pos < body.size()) {
    size_t nl = body.find('\n', pos);
    std::string line = body.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    pos = nl == std::string::npos ? body.size() : nl + 1;
    if (trim(line).empty() || line.find('|') == std::string::npos) continue;
    std::vector<std::string> c = cells(line);
    if (c.size() < 7) continue;
    if (header) { header = false; continue; }      // column titles
    if (c[0].find("---") != std::string::npos) continue;  // separator row
    Row r{};
    r.name = c[0];
    const char* s = c[2].c_str();
    TEST_ASSERT_TRUE_MESSAGE(firstNumber(s, r.bandLo) && firstNumber(s, r.bandHi), c[2].c_str());
    s = c[3].c_str();
    TEST_ASSERT_TRUE_MESSAGE(firstNumber(s, r.freq), c[3].c_str());
    s = c[4].c_str();
    TEST_ASSERT_TRUE_MESSAGE(firstNumber(s, r.power), c[4].c_str());
    s = c[5].c_str();
    TEST_ASSERT_TRUE_MESSAGE(firstNumber(s, r.duty), c[5].c_str());
    std::string x = c[6];
    TEST_ASSERT_TRUE_MESSAGE(x.rfind("yes", 0) == 0 || x.rfind("no", 0) == 0, x.c_str());
    r.experimental = x.rfind("yes", 0) == 0;
    rows.push_back(r);
  }
  return rows;
}

}  // namespace

void test_doc_region_table_matches_code() {
  std::string doc;
  TEST_ASSERT_TRUE_MESSAGE(readDoc(doc), "docs/en/frequencies.md not found");
  std::vector<Row> rows = parseTable(doc);
  TEST_ASSERT_EQUAL_UINT32_MESSAGE(regionCount(), rows.size(), "row count differs from regionCount()");
  for (size_t i = 0; i < rows.size(); i++) {
    const RegionInfo& ri = regionAt(i);
    const Row& r = rows[i];
    char msg[96];
    snprintf(msg, sizeof msg, "row %u (%s)", (unsigned)i, r.name.c_str());
    TEST_ASSERT_EQUAL_STRING_MESSAGE(ri.name, r.name.c_str(), msg);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, ri.freqStart, (float)r.bandLo, msg);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, ri.freqEnd, (float)r.bandHi, msg);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0006f, regionFreqMHz(ri.id, "LongFast", 250.0f), (float)r.freq, msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ri.powerLimit, (int)lround(r.power), msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ri.dutyCycle, (int)lround(r.duty), msg);
    TEST_ASSERT_EQUAL_MESSAGE(ri.experimental, r.experimental, msg);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_doc_region_table_matches_code);
  return UNITY_END();
}
