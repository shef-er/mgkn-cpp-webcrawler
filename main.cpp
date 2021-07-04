#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <vector>
#include <regex>
#include <queue>
#include <thread>
#include <mutex>

class Curler {
    private:
        static size_t CURLWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
        {
            ((std::string*)userp)->append((char*)contents, size * nmemb);
            return size * nmemb;
        }

        static std::string curlRun(const std::string& url)
        {
            std::string readBuffer;

            CURL *curl;
            curl = curl_easy_init();

            if (curl) {
                const char *cstr_url = url.c_str();
                curl_easy_setopt(curl, CURLOPT_URL, cstr_url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CURLWriteCallback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
            }
            curl_global_cleanup();

            return readBuffer;
        }

        static std::vector<std::string> findLinks(std::string buffer)
        {
            std::vector<std::string> links;
            std::smatch m;
            std::regex e ("href=\"(/[^\"]*)\"");   // matches words by pattern: href="%s"

            unsigned i = 0;
            while (std::regex_search (buffer, m, e)) {
                links.push_back(m[1]);
                buffer = m.suffix().str();
                ++i;
            }

            return links;
        }

    public:
        static std::vector<std::string> parseLinksFromUrl(const std::string& url)
        {
            std::string readBuffer = curlRun(url);
            std::vector<std::string> links = findLinks(readBuffer);

            //  Logging
//            for (const auto& x: links) {
//                std::cout << " - " << x << std::endl;
//            }
            return links;
        }
};
// ###########################################################

std::mutex activeListGuard;
std::mutex passiveListGuard;

using urlPair = std::pair<std::string, unsigned int>;
using urlPairQueue = std::deque<urlPair>;

urlPairQueue active;
std::deque<std::string> passive;

void threadWorker(unsigned int retryCount, unsigned int maxDepth)
{
    urlPairQueue temp;

    while (true) {
        urlPair record;

        for (int i = 0; i < retryCount; ++i) {
            if (activeListGuard.try_lock()) {

                for (const auto& t: temp) {
                    active.push_back(t);
                }
                temp.clear();

                if (!active.empty()) {
                    record = active.front();
                    active.pop_front();

                    std::cout << std::endl << "active picked: (" << record.first << "," << record.second << ")" << std::endl;
                }
                activeListGuard.unlock();
                break;
            } else {
                continue;
            }
        }

        if (record.first.empty()) {
            return;
        }

        if (record.second >= maxDepth) {
            std::cout << "reached maxDepth, skipping..." << std::endl;
            continue;
        }

        if (std::find(passive.begin(), passive.end(), record.first) != passive.end()) {
            std::cout << "double url, skipping..." << std::endl;
            continue;
        }

        std::string recordUrl = record.first;
        std::vector<std::string> relativeUrlList = Curler::parseLinksFromUrl(recordUrl);
        for (const auto& x: relativeUrlList) {
            temp.push_back(std::make_pair(x, record.second + 1));
        }

        for (int i = 0; i < retryCount; ++i) {
            if (passiveListGuard.try_lock()) {
                passive.push_back(recordUrl);
                passiveListGuard.unlock();
                break;
            }
        }

        std::cout << "passive added: " << passive.back() << std::endl;
        std::cout << "A:" << active.size() << " P:" << passive.size() << std::endl;

    }
}

void writeToFile(const std::deque<std::string>& list)
{
    std::ofstream outputFile;
    outputFile.open ("output.txt");

    for (const auto& x: list) {
        outputFile << x << std::endl;
    }

    outputFile.close();
}

int main()
{
    active.push_back(std::make_pair("https://www.google.com", 0));
    active.push_back(std::make_pair("https://www.google.com", 0));
    active.push_back(std::make_pair("https://yandex.ru", 0));
    active.push_back(std::make_pair("https://yandex.ru", 0));

    unsigned int maxThreads = std::thread::hardware_concurrency();
    unsigned int maxDepth = 2;

    //   !!!  DO NOT CHANGE ANYTHING BELOW THIS !!!

    std::vector<std::thread> threads(maxThreads);

    for (int i = 0; i < maxThreads; ++i) {
        threads[i] = std::thread(threadWorker, maxThreads/2, maxDepth);
    }

    for (int i = 0; i < maxThreads; ++i) {
        threads[i].join();
    }

    writeToFile(passive);

    return 0;
}
