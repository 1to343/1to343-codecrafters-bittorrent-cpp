#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

bool isEncodedNum(const std::string& encoded_value) {
  if (encoded_value[0] != 'i') {
    return false;
  }
  if (encoded_value[encoded_value.size() - 1] != 'e') {
    return false;
  }
  return encoded_value.size() > 2;
}

bool isEncodedList(const std::string& encoded_value) {
  return encoded_value[0] == 'l' &&
         encoded_value[encoded_value.size() - 1] == 'e';
}

bool isEncodedDict(const std::string& encoded_value) {
  return encoded_value[0] == 'd' &&
         encoded_value[encoded_value.size() - 1] == 'e';
}

json decodeBencodedString(const std::string& encoded_value, uint& index) {
  size_t colon_index = encoded_value.find(':', index);
  if (colon_index != std::string::npos) {
    std::string number_string =
        encoded_value.substr(index, colon_index - index);
    int64_t number = std::atoll(number_string.c_str());
    std::string str = encoded_value.substr(colon_index + 1, number);
    index = colon_index + str.size() + 1;
    return json{str};
  } else {
    throw std::runtime_error("Invalid encoded value: " + encoded_value);
  }
}

json decodeBencodedNum(const std::string& encoded_value, uint& index) {
  uint last = encoded_value.find('e', index);
  const std::string number_str =
      encoded_value.substr(index + 1, last - index - 1);
  index = last + 1;
  const long long number = std::stoll(number_str);
  return json{number};
}

json decodeBencodedList(const std::string& encoded_value, uint& index) {
  json arr = json::array();
  while (index < encoded_value.size() && encoded_value[index] != 'e') {
    json ans;
    if (encoded_value[index] == 'i') {
      ans = decodeBencodedNum(encoded_value, index);
    } else if (encoded_value[index] != 'l') {
      ans = decodeBencodedString(encoded_value, index);
    } else {
      std::string encoded_substr = encoded_value.substr(index);
      uint tmp_index = 1;
      ans = decodeBencodedList(encoded_substr, tmp_index);
      index += tmp_index;
    }
    arr.push_back(ans);
  }
  ++index;
  return arr;
}

json decodeBencodedDict(const std::string& encoded_value, uint& index) {
  json dict = json::object();
  bool done = false;
  json first_val;
  json second_val;
  while (index < encoded_value.size() && encoded_value[index] != 'e') {
    if (encoded_value[index] == 'i') {
      second_val = decodeBencodedNum(encoded_value, index);
      dict[first_val] = second_val;
      done = false;
    } else if (encoded_value[index] == 'l') {
      ++index;
      second_val = decodeBencodedList(encoded_value, index);
      json tmp = second_val;
      auto got = tmp.dump();
      dict[first_val] = second_val;
      done = false;
    } else if (encoded_value[index] == 'd') {
      ++index;
      second_val = decodeBencodedDict(encoded_value, index);
      dict[first_val] = second_val;
      done = false;
    } else {
      if (done) {
        second_val = decodeBencodedString(encoded_value, index);
        dict[first_val] = second_val;
        done = false;
      } else {
        first_val = decodeBencodedString(encoded_value, index);
        done = true;
      }
    }
  }
  ++index;
  return dict;
}


json decodeBencodedValue(const std::string& encoded_value) {
  uint index = 1;
  if (std::isdigit(encoded_value[0])) {
    // Example: "5:hello" -> "hello"
    --index;
    return decodeBencodedString(encoded_value, index);
  } else if (isEncodedNum(encoded_value)) {
    index = 0;
    return decodeBencodedNum(encoded_value, index);
  } else if (isEncodedList(encoded_value)) {
    return decodeBencodedList(encoded_value, index);
  } else if (isEncodedDict(encoded_value)) {
    return decodeBencodedDict(encoded_value, index);
  } else {
    throw std::runtime_error("Unhandled encoded value: " + encoded_value);
  }
}

std::string bencodeTheString(const json& info) {
  std::string ans;
  if (info.is_object()) {
    std::map<std::string, json> data = info;
    ans += 'd';
    for (auto& [key, value] : data) {
      ans += bencodeTheString(key);
      ans += bencodeTheString(value);
    }
    ans += 'e';
  } else if (info.is_array()) {
    std::vector<json> data = info;
    ans += 'l';
    for (auto& item : data) {
      ans += bencodeTheString(item);
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

void stringToSHA1(const std::string& data,
                  std::array<unsigned char, SHA_DIGEST_LENGTH>& hash) {
  SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
       hash.data());
}

void getPiecesHashes(const json& info, std::vector<std::string>& res) {
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

std::vector<std::string> extractInfo(const std::string& buffer) {
  std::vector<std::string> res;
  std::array<unsigned char, SHA_DIGEST_LENGTH> info_hash{};
  auto torrent = decodeBencodedValue(buffer);
  std::string announce = torrent["announce"];
  res.push_back("Tracker URL: " + announce);
  auto info = torrent["info"];
  std::string length = std::to_string(info["length"].template get<int>());
  res.push_back("Length: " + length);
  stringToSHA1(bencodeTheString(info), info_hash);
  std::stringstream ss;
  for (auto c : info_hash) {
    ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);
  }
  res.push_back("Info Hash: " + ss.str());
  std::string piece_length =
      std::to_string(info["piece length"].template get<int>());
  res.push_back("Piece Length: " + piece_length);
  res.emplace_back("Pieces Hashes:");
  getPiecesHashes(info, res);
  return res;
}

std::vector<std::string> parseTorrentFile(const std::string& filename) {
  std::vector<std::string> res;
  std::fstream fs;
  fs.open(filename, std::ios::in | std::ios::binary);
  if (!fs.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }
  std::istreambuf_iterator<char> it{fs}, end;
  std::string buffer(it, end);
  res = extractInfo(buffer);
  return res;
}

json openTorrentFile(const std::string& filename) {
  std::fstream fs;
  fs.open(filename, std::ios::in | std::ios::binary);
  if (!fs.is_open()) {
    throw std::runtime_error("Cannot open file: " + filename);
  }
  std::istreambuf_iterator<char> it{fs}, end;
  std::string buffer(it, end);
  auto torrent = decodeBencodedValue(buffer);
  fs.close();
  return torrent;
}


unsigned int getNum(unsigned char c) {
  unsigned int ans;
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);
  ss >> ans;
  return ans;
}

std::vector<std::string> getAns(const std::string& peers) {
  class ip {
   public:
    std::vector<uint> nums;
    std::string get_str() {
      std::string ans;
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
      ip_addr.nums.push_back(getNum(j));
    }
    ans.push_back(ip_addr.get_str());
  }
  return ans;
}

std::vector<std::string> sendRequest(const std::string& filename) {
  CURL* curl = curl_easy_init();
  std::array<unsigned char, SHA_DIGEST_LENGTH> hash{};
  if (!curl) {
    std::cerr << "Failed to initialize cURL" << std::endl;
    return {};
  }
  auto torrent = openTorrentFile(filename);
  std::string url = torrent["announce"];
  auto info = torrent["info"];
  std::string bencoded_info = bencodeTheString(info);
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
  auto write_callback =
      +[](char* contents, size_t size, size_t nmemb, void* userp) -> size_t {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
  };
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode status = curl_easy_perform(curl);
  if (status != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(status)
              << std::endl;
  }
  curl_free(encoded_info_hash);
  curl_easy_cleanup(curl);

  auto data = decodeBencodedValue(response);
  std::string peers = data.at("peers").template get<std::string>();
  getNum(peers[0]);
  return getAns(peers);
}

std::string getInfoHash(const std::string& filename) {
  std::array<unsigned char, SHA_DIGEST_LENGTH> hash{};

  auto torrent = openTorrentFile(filename);
  auto info = torrent["info"];
  std::string bencoded_info = bencodeTheString(info);
  SHA1(reinterpret_cast<const unsigned char*>(bencoded_info.c_str()),
       bencoded_info.size(), hash.data());
  std::string raw_hash;
  std::copy(hash.begin(),hash.end(),std::back_insert_iterator<std::string>(raw_hash));
  return raw_hash;
}

void insertData(const std::string& part, std::vector<unsigned char>& msg) {
  for (const auto& i : part) {
    msg.push_back(i);
  }
}


void establishConnection(const std::string& filename, const std::string& peer) {
  int client_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (client_socket == -1) {
    std::cerr << "Error creating socket" << std::endl;
    return;
  }
  uint delim = peer.find(':');
  sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(std::stoi(peer.substr(delim + 1)));
  if (inet_pton(AF_INET, peer.substr(0, delim).c_str(), &(server_address.sin_addr)) <= 0) {
    std::cerr << "Error converting IP address" << std::endl;
    close(client_socket);
    return;
  }
  if (connect(client_socket, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) == -1) {
    std::cerr << "Error connecting to the server" << std::endl;
    close(client_socket);
    return;
  }
  std::string info_hash = getInfoHash(filename);
  unsigned char length = 19;
  std::string protocol = "BitTorrent protocol";
  std::array<unsigned char, 8> reserved{};
  reserved.fill(0);
  std::string peer_id = "00112233445566778899";
  std::vector<unsigned char> msg;
  msg.push_back(length);
  insertData(protocol, msg);
  for (const auto& i: reserved) {
    msg.push_back(i);
  }
  insertData(info_hash, msg);
  insertData(peer_id, msg);
  if (send(client_socket, msg.data(), msg.size(), 0) == -1) {
    std::cerr << "Error sending data" << std::endl;
  }
  char buffer[msg.size()];
    ssize_t bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytesRead == -1) {
      std::cerr << "Error receiving data" << std::endl;
    } else {
      buffer[bytesRead] = '\0';
    }
    std::string recv_peer_id(buffer + 48, buffer + 68);
    std::stringstream ss;
    for (unsigned char c : recv_peer_id) {
      ss << std::hex << std::setfill('0') << std::setw(2)
         << static_cast<int>(c);
    }
    std::cout << "Peer ID: " << ss.str() << '\n';
  close(client_socket);
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
    json decoded_value = decodeBencodedValue(encoded_value);
    std::cout << decoded_value.dump() << std::endl;
  } else if (command == "info") {
    if (argc < 3) {
      std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
      return 1;
    }
    std::string file = argv[2];
    std::vector<std::string> info = parseTorrentFile(file);
    for (const auto& i : info) {
      std::cout << i << '\n';
    }
  } else if (command == "peers") {
    if (argc < 3) {
      std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
      return 1;
    }
    std::string file = argv[2];
    auto peers = sendRequest(file);
    for (const auto& peer : peers) {
      std::cout << peer << '\n';
    }
  } else if (command == "handshake") {
    if (argc < 4) {
      std::cerr << "Usage: " << argv[0] << " info <file>" << std::endl;
      return 1;
    }
    std::string file = argv[2];
    std::string peer = argv[3];
    establishConnection(file, peer);
  }else {
    std::cerr << "unknown command: " << command << std::endl;
    return 1;
  }
  return 0;
}

// int main() {
//   std::string filename = "sample.torrent";
//   std::string peer = "178.62.82.89:51470";
//   establishConnection(filename, peer);
//
//   std::cout << ans.dump() << '\n';
// }

//sample.torrent
// 178.62.82.89:51470

