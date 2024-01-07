#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

bool is_encoded_num(const std::string& encoded_value) {
  if (encoded_value[0] != 'i') {
    return false;
  }
  if (encoded_value[encoded_value.size() - 1] != 'e') {
    return false;
  }
  return encoded_value.size() > 2;
}

bool is_encoded_list(const std::string& encoded_value) {
  return encoded_value[0] == 'l' &&
         encoded_value[encoded_value.size() - 1] == 'e';
}

json decode_bencoded_string(const std::string& encoded_value) {
  size_t colon_index = encoded_value.find(':');
  if (colon_index != std::string::npos) {
    std::string number_string = encoded_value.substr(0, colon_index);
    int64_t number = std::atoll(number_string.c_str());
    std::string str = encoded_value.substr(colon_index + 1, number);
    return json(str);
  } else {
    throw std::runtime_error("Invalid encoded value: " + encoded_value);
  }
}

json decode_bencoded_number(const std::string& encoded_value) {
  const std::string number_str =
      encoded_value.substr(1, encoded_value.size() - 2);
  const long long number = std::stoll(number_str);
  return json(number);
}

std::string get_elem(const std::string& encoded_value, uint& index) {
  std::string element;
  json ans;
  if (encoded_value[index] == 'i') {
    uint last = encoded_value.find("e", index + 1);
    element = encoded_value.substr(index, last - index + 1);
    ans = decode_bencoded_number(element);
    index = last + 1;
  } else if (encoded_value[index] != 'l') {
    long long len = 0;
    uint delim_pos = encoded_value.find(":", index + 1);
    std::string len_str = encoded_value.substr(index, delim_pos - index);
    len = stoll(len_str);
    element = encoded_value.substr(index, delim_pos - index + len + 1);
    ans = decode_bencoded_string(element);
    index += delim_pos - index + len + 1;
  } else {
    std::string encoded_substr = encoded_value.substr(index);
  }
  return element;
}

json decode_bencoded_list(const std::string& encoded_value, uint& index) {
//  uint index = 1;
  json arr = json::array();
  while (encoded_value[index] != 'e') {
    std::string element;
    json ans;
    if (encoded_value[index] == 'i') {
      uint last = encoded_value.find("e", index + 1);
      element = encoded_value.substr(index, last - index + 1);
      ans = decode_bencoded_number(element);
      index = last + 1;
    } else if (encoded_value[index] != 'l') {
      long long len = 0;
      uint delim_pos = encoded_value.find(":", index + 1);
      std::string len_str = encoded_value.substr(index, delim_pos - index);
      len = stoll(len_str);
      element = encoded_value.substr(index, delim_pos - index + len + 1);
      ans = decode_bencoded_string(element);
      index += delim_pos - index + len + 1;
    } else {
      std::string encoded_substr = encoded_value.substr(index);
      uint tmp_index = 1;
      ans = decode_bencoded_list(encoded_substr, tmp_index);
      index += tmp_index;
    }
    arr.push_back(ans);
  }
  ++index;
  return arr;
}

json decode_bencoded_value(const std::string& encoded_value) {
  if (std::isdigit(encoded_value[0])) {
    // Example: "5:hello" -> "hello"
    return decode_bencoded_string(encoded_value);
  } else if (is_encoded_num(encoded_value)) {
    return decode_bencoded_number(encoded_value);
  } else if (is_encoded_list(encoded_value)) {
    uint index = 1;
    return decode_bencoded_list(encoded_value, index);
  } else {
    throw std::runtime_error("Unhandled encoded value: " + encoded_value);
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
    return 1;
  }

  std::string command = argv[1];

  if (command == "decode") {
    if (argc < 3) {
      std::cerr << "Usage: " << argv[0] << " decode <encoded_value>"
                << std::endl;
      return 1;
    }
    // You can use print statements as follows for debugging, they'll be visible
    // when running tests.
    //    std::cout << "Logs from your program will appear here!" << std::endl;

    // Uncomment this block to pass the first stage
    std::string encoded_value = argv[2];
    json decoded_value = decode_bencoded_value(encoded_value);
    std::cout << decoded_value.dump() << std::endl;
  } else {
    std::cerr << "unknown command: " << command << std::endl;
    return 1;
  }

  return 0;
}
// ll5:helloi1ee4:shiti2ee