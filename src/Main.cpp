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

bool is_encoded_dict(const std::string& encoded_value) {
  return encoded_value[0] == 'd' &&
         encoded_value[encoded_value.size() - 1] == 'e';
}

json decode_bencoded_string(const std::string& encoded_value, uint& index) {
//  std::cout << index << " str\n";
  size_t colon_index = encoded_value.find(':', index);
  if (colon_index != std::string::npos) {
    std::string number_string =
        encoded_value.substr(index, colon_index - index);
    int64_t number = std::atoll(number_string.c_str());
    std::string str = encoded_value.substr(colon_index + 1, number);
    index = colon_index + str.size() + 1;
    return json(str);
  } else {
    throw std::runtime_error("Invalid encoded value: " + encoded_value);
  }
}

json decode_bencoded_number(const std::string& encoded_value, uint& index) {
//  std::cout << index << " num\n";
  uint last = encoded_value.find('e', index);
  const std::string number_str =
      encoded_value.substr(index + 1, last - index - 1);
  index = last + 1;
  const long long number = std::stoll(number_str);
  return json(number);
}

json decode_bencoded_list(const std::string& encoded_value, uint& index) {
  json arr = json::array();
  while (encoded_value[index] != 'e') {
    json ans;
    if (encoded_value[index] == 'i') {
      ans = decode_bencoded_number(encoded_value, index);
    } else if (encoded_value[index] != 'l') {
      ans = decode_bencoded_string(encoded_value, index);
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

json decode_bencoded_dict(const std::string& encoded_value, uint& index) {
  json dict;
  bool done = false;
  json first_val;
  json second_val;
  while (encoded_value[index] != 'e') {
    if (encoded_value[index] == 'i') {
      second_val = decode_bencoded_number(encoded_value, index);
      auto first = first_val.dump();
      auto second = second_val.dump();
      //      std::cout << "first and second " << first << ' ' << second <<
      //      '\n';
      dict[first] = second;
      done = false;
    } else if (encoded_value[index] == 'l') {
      second_val = decode_bencoded_list(encoded_value, index);
      auto first = first_val.dump();
      auto second = second_val.dump();
      //      std::cout << "first and second " << first << ' ' << second <<
      //      '\n';
      dict[first] = second;
      done = false;
    } else {
      if (done) {
        second_val = decode_bencoded_string(encoded_value, index);
        auto first = first_val.dump();
        auto second = second_val.dump();
        //        std::cout << "first and second " << first << ' ' << second <<
        //        '\n';
        dict[first] = second;
        done = false;
      } else {
        first_val = decode_bencoded_string(encoded_value, index);
        //        json tmp = first_val;
        //        auto first = tmp.dump();
        //        std::cout << "only first " << first << '\n';
        done = true;
      }
    }
  }
  ++index;
  return dict;
}

json decode_bencoded_value(const std::string& encoded_value) {
  uint index = 1;
  if (std::isdigit(encoded_value[0])) {
    // Example: "5:hello" -> "hello"
    return decode_bencoded_string(encoded_value, index);
  } else if (is_encoded_num(encoded_value)) {
    index = 0;
    return decode_bencoded_number(encoded_value, index);
  } else if (is_encoded_list(encoded_value)) {
    return decode_bencoded_list(encoded_value, index);
  } else if (is_encoded_dict(encoded_value)) {
    return decode_bencoded_dict(encoded_value, index);
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
        //    std::cout << "Logs from your program will appear here!" <<
//        std::endl;

    // Uncomment this block to pass the first_val stage
    std::string encoded_value = argv[2];
    json decoded_value = decode_bencoded_value(encoded_value);
    std::cout << decoded_value.dump() << std::endl;
  } else {
    std::cerr << "unknown command: " << command << std::endl;
    return 1;
  }

  return 0;
}

// int main() {
//   std::string value;
//   std::cin >> value;
//   json ans = decode_bencoded_value(value);
//   std::cout << ans.dump() << '\n';
// }
//  ll5:helloi1ee4:shiti2ee