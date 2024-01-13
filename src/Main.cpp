#include <curl/curl.h>
#include <openssl/sha.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
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
  uint last = encoded_value.find('e', index);
  const std::string number_str =
      encoded_value.substr(index + 1, last - index - 1);
  index = last + 1;
  const long long number = std::stoll(number_str);
  return json(number);
}

json decode_bencoded_list(const std::string& encoded_value, uint& index) {
  json arr = json::array();
  while (index < encoded_value.size() && encoded_value[index] != 'e') {
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
  json dict = json::object();
  bool done = false;
  json first_val;
  json second_val;
  while (index < encoded_value.size() && encoded_value[index] != 'e') {
    if (encoded_value[index] == 'i') {
      second_val = decode_bencoded_number(encoded_value, index);
      dict[first_val] = second_val;
      done = false;
    } else if (encoded_value[index] == 'l') {
      ++index;
      second_val = decode_bencoded_list(encoded_value, index);
      json tmp = second_val;
      auto got = tmp.dump();
      dict[first_val] = second_val;
      done = false;
    } else if (encoded_value[index] == 'd') {
      ++index;
      second_val = decode_bencoded_dict(encoded_value, index);
      dict[first_val] = second_val;
      done = false;
    } else {
      if (done) {
        second_val = decode_bencoded_string(encoded_value, index);
        dict[first_val] = second_val;
        done = false;
      } else {
        first_val = decode_bencoded_string(encoded_value, index);
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
    --index;
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

std::string bencode_the_string(json info) {
  std::string ans = "";
  if (info.is_object()) {
    std::map<std::string, json> data = info;
    ans += 'd';
    for (auto& [key, value] : data) {
      ans += bencode_the_string(key);
      ans += bencode_the_string(value);
    }
    ans += 'e';
  } else if (info.is_array()) {
    std::vector<json> data = info;
    ans += 'l';
    for (auto& item : data) {
      ans += bencode_the_string(item);
    }
    ans += 'e';
  } else if (info.is_number()) {
    long long data = info;
    ans += 'i';
    ans += std::to_string(data);
    ans += 'e';
  } else if (info.is_string()) {
    std::string data = info;
    ans += std::to_string(data.size());
    ans += ':';
    ans += data;
  } else {
    throw std::runtime_error("JSON type not handled");
  }
  return ans;
}

void string_to_sha1(const std::string& data,
                    std::array<unsigned char, SHA_DIGEST_LENGTH>& hash) {
  SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
       hash.data());
}

void get_pieces_hashes(const json& info, std::vector<std::string>& res) {
  std::string pieces_string = info["pieces"];
  std::vector<std::string> pieces_hashes;
  for (uint64_t i = 0; i < pieces_string.size(); i += 20) {
    pieces_hashes.push_back(pieces_string.substr(i, 20));
  }
  std::stringstream ss;
  for (const auto& hash : pieces_hashes) {
    ss.str("");
    for (unsigned char c : hash) {
      ss << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<int>(c);
    }
    if (ss.str().length() != 40) {
      throw std::runtime_error("Wrong piece hash length: " + ss.str());
    }
    res.push_back(ss.str());
  }
}

std::vector<std::string> extract_info(const std::string& buffer) {
  std::vector<std::string> res;
  std::array<unsigned char, SHA_DIGEST_LENGTH> info_hash;
  auto torrent = decode_bencoded_value(buffer);
  std::string announce = torrent["announce"];
  res.push_back("Tracker URL: " + announce);
  auto info = torrent["info"];
  std::string length = std::to_string(info["length"].template get<int>());
  res.push_back("Length: " + length);
  string_to_sha1(bencode_the_string(info), info_hash);
  std::stringstream ss;
  for (auto c : info_hash) {
    ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);
  }
  res.push_back("Info Hash: " + ss.str());
  std::string piece_length =
      std::to_string(info["piece length"].template get<int>());
  res.push_back("Piece Length: " + piece_length);
  res.push_back("Pieces Hashes:");
  get_pieces_hashes(info, res);
  return res;
}

std::vector<std::string> parse_torrent_file(const std::string& filename) {
  std::vector<std::string> res;
  std::fstream fs;
  fs.open(filename, std::ios::in | std::ios::binary);
  if (!fs.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }
  std::istreambuf_iterator<char> it{fs}, end;
  std::string buffer(it, end);
  res = extract_info(buffer);
  return res;
}

json open_torrent_file(std::string filename) {
  std::fstream fs;
  fs.open(filename, std::ios::in | std::ios::binary);
  if (!fs.is_open()) {
    throw std::runtime_error("Cannot open file: "+filename);
  }
  std::istreambuf_iterator<char> it{fs},end;
  std::string buffer(it,end);
  auto torrent = decode_bencoded_value(buffer);
  fs.close();
  return torrent;
}

int get_num(unsigned char c) {
  unsigned int ans;
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);
  ss >> ans;
  return ans;
}

std::vector<std::string> get_ans(const std::string& peers) {
  class ip {
   public:
    std::vector<uint> nums;
    std::string get_str() {
      std::string ans;
      uint last = 0;
      for (size_t i = 0; i < 4; ++i) {
        ans += std::to_string(nums[i]);
        if (i < 3) {
          ans += '.';
        } else {
          ans += ':';
        }
      }
      ans += std::to_string(nums[4] * 256 + nums[5]);
      return ans;
    }
  };
  ip ip_addr;
  std::vector<std::string> ans;
  for (size_t i = 0; i < peers.size(); i += 6) {
    std::string tmp = peers.substr(i, 6);
    ip_addr.nums.clear();
    for (const auto& j : tmp) {
      ip_addr.nums.push_back(get_num(j));
    }
    ans.push_back(ip_addr.get_str());
  }
  return ans;
}

std::vector<std::string> send_request(const std::string& filename) {
  CURL* curl = curl_easy_init();
  std::array<unsigned char, SHA_DIGEST_LENGTH> hash;
  if (!curl) {
    std::cerr << "Failed to initialize cURL" << std::endl;
    return {};
  }
  auto torrent = open_torrent_file(filename);
  std::string url = torrent["announce"];
  auto info = torrent["info"];
  std::string bencoded_info = bencode_the_string(info);
  std::string peer_id = "00112233445566778899";
  size_t port = 6881;
  size_t uploaded = 0;
  size_t downloaded = 0;
  size_t left = std::stoul(std::to_string(info["length"].template get<int>()));
  size_t compact = 1;
  std::string response;
  url += "?info_hash=";
  SHA1(reinterpret_cast<const unsigned char*>(bencoded_info.c_str()),
       bencoded_info.size(), hash.data());
  char* encoded_info_hash = curl_easy_escape(
      curl, reinterpret_cast<const char*>(hash.data()), SHA_DIGEST_LENGTH);
  url += std::string(encoded_info_hash);
  url += "&peer_id=";
  url += peer_id;
  url += "&port=";
  url += std::to_string(port);
  url += "&uploaded=";
  url += std::to_string(uploaded);
  url += "&downloaded=";
  url += std::to_string(downloaded);
  url += "&left=";
  url += std::to_string(left);
  url += "&compact=";
  url += std::to_string(compact);
//  std::cout << url << " url\n";
  auto write_callback = +[](char *contents, size_t size, size_t nmemb, void *userp)-> size_t {
    ((std::string*)userp)->append((char*)contents,size*nmemb);
    return size*nmemb;
  };
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl,CURLOPT_WRITEDATA, &response);
  CURLcode status = curl_easy_perform(curl);
  if (status != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(status) << std::endl;
  }
  curl_free(encoded_info_hash);
  curl_easy_cleanup(curl);

  auto data = decode_bencoded_value(response);
  std::string peers = data.at("peers").template get<std::string>();
  get_num(peers[0]);
  return get_ans(peers);
//  std::cout << peers << '\n';
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
    std::string encoded_value = argv[2];
    json decoded_value = decode_bencoded_value(encoded_value);
    std::cout << decoded_value.dump() << std::endl;
  } else if (command == "info") {
    if (argc < 3) {
      std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
      return 1;
    }
    std::string file = argv[2];
    std::vector<std::string> info = parse_torrent_file(file);
    for (const auto& i : info) {
      std::cout << i << '\n';
    }
  } else if (command == "peers") {
    if (argc < 3) {
      std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
      return 1;
    }
    std::string file = argv[2];
    auto peers = send_request(file);
    for (auto peer : peers) {
      std::cout << peer << '\n';
    }
  } else {
      std::cerr << "unknown command: " << command << std::endl;
      return 1;
  }

  return 0;
}

// int main() {
//   std::string value;
//   std::cin >> value;
//   int len = value.size();
//   json ans = decode_bencoded_value(value);
//   std::cout << ans.dump() << '\n';
// }
