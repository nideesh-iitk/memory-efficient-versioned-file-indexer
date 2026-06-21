#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

using WordFrequencyMap = unordered_map<string, long long>;

static char toLowerChar(char ch) {
    return static_cast<char>(tolower(static_cast<unsigned char>(ch)));
}

static string normalizeWord(string word) {
    for (char& ch : word) {
        ch = toLowerChar(ch);
    }
    return word;
}

class BufferedFileReader {
private:
    ifstream file;
    vector<char> buffer;
    size_t bufferSize;

public:
    BufferedFileReader(const string& filename, size_t sizeKB)
        : bufferSize(sizeKB * 1024) {
        if (sizeKB < 256 || sizeKB > 1024) {
            throw invalid_argument("Buffer size must be between 256KB and 1024KB");
        }

        file.open(filename, ios::binary);
        if (!file.is_open()) {
            throw runtime_error("Cannot open input file: " + filename);
        }

        buffer.resize(bufferSize);
    }

    bool readChunk(string& outChunk) {
        if (!file.good()) {
            return false;
        }

        file.read(buffer.data(), static_cast<streamsize>(bufferSize));
        streamsize bytesRead = file.gcount();

        if (bytesRead <= 0) {
            return false;
        }

        outChunk.assign(buffer.data(), static_cast<size_t>(bytesRead));
        return true;
    }
};

class WordTokenizer {
private:
    string leftover;

public:
    void tokenize(const string& chunk, WordFrequencyMap& freq) {
        string word = leftover;
        leftover.clear();

        for (char ch : chunk) {
            if (isalnum(static_cast<unsigned char>(ch))) {
                word += toLowerChar(ch);
            } else if (!word.empty()) {
                ++freq[word];
                word.clear();
            }
        }

        leftover = word;
    }

    void flush(WordFrequencyMap& freq) {
        if (!leftover.empty()) {
            ++freq[leftover];
            leftover.clear();
        }
    }
};

class VersionedWordIndex {
private:
    unordered_map<string, WordFrequencyMap> data;

public:
    void indexFile(const string& filename, const string& version, size_t bufferKB) {
        WordFrequencyMap& freq = data[version];
        indexFile(filename, bufferKB, freq);
    }

    void indexFile(const string& filename, size_t bufferKB, WordFrequencyMap& freq) {
        BufferedFileReader reader(filename, bufferKB);
        WordTokenizer tokenizer;
        string chunk;

        while (reader.readChunk(chunk)) {
            tokenizer.tokenize(chunk, freq);
        }
        tokenizer.flush(freq);
    }

    long long getWordCount(const string& version, const string& word) const {
        auto versionIt = data.find(version);
        if (versionIt == data.end()) {
            return 0;
        }

        auto wordIt = versionIt->second.find(normalizeWord(word));
        if (wordIt == versionIt->second.end()) {
            return 0;
        }

        return wordIt->second;
    }

    vector<pair<string, long long>> getTopK(const string& version, int k) const {
        vector<pair<string, long long>> result;
        if (k <= 0) {
            return result;
        }

        auto versionIt = data.find(version);
        if (versionIt == data.end()) {
            return result;
        }

        result.reserve(versionIt->second.size());
        for (const auto& entry : versionIt->second) {
            result.push_back(entry);
        }

        sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second) {
                return a.second > b.second;
            }
            return a.first < b.first;
        });

        if (static_cast<size_t>(k) < result.size()) {
            result.resize(static_cast<size_t>(k));
        }

        return result;
    }
};

template <typename T>
void printResult(const T& value) {
    cout << value << '\n';
}

class Query {
public:
    virtual void execute(VersionedWordIndex& index) = 0;
    virtual ~Query() = default;
};

class WordQuery : public Query {
private:
    string version;
    string word;

public:
    WordQuery(string versionName, string targetWord)
        : version(move(versionName)), word(normalizeWord(move(targetWord))) {}

    void execute(VersionedWordIndex& index) override {
        cout << "Version: " << version << '\n';
        cout << word << " : ";
        printResult(index.getWordCount(version, word));
    }
};

class TopKQuery : public Query {
private:
    string version;
    int k;

public:
    TopKQuery(string versionName, int topK)
        : version(move(versionName)), k(topK) {}

    void execute(VersionedWordIndex& index) override {
        auto results = index.getTopK(version, k);
        cout << "Top-" << k << " words in version " << version << ":\n";
        for (const auto& entry : results) {
            cout << entry.first << " : " << entry.second << '\n';
        }
    }
};

class DiffQuery : public Query {
private:
    string version1;
    string version2;
    string word;

public:
    DiffQuery(string firstVersion, string secondVersion, string targetWord)
        : version1(move(firstVersion)),
          version2(move(secondVersion)),
          word(normalizeWord(move(targetWord))) {}

    void execute(VersionedWordIndex& index) override {
        long long count1 = index.getWordCount(version1, word);
        long long count2 = index.getWordCount(version2, word);
        cout << "Difference (" << version2 << " - " << version1 << "): ";
        printResult(count2 - count1);
    }
};

static void printUsage() {
    cerr << "Usage:\n"
         << "  Word query: analyzer --file <path> --version <name> --buffer <256-1024> --query word --word <token>\n"
         << "  Top-K query: analyzer --file <path> --version <name> --buffer <256-1024> --query top --top <k>\n"
         << "  Diff query: analyzer --file1 <path> --version1 <name> --file2 <path> --version2 <name> --buffer <256-1024> --query diff --word <token>\n";
}

static string nextArg(int& i, int argc, char* argv[], const string& flag) {
    if (i + 1 >= argc) {
        throw invalid_argument("Missing value for argument: " + flag);
    }
    return argv[++i];
}

int main(int argc, char* argv[]) {
    try {
        string file1;
        string file2;
        string version1;
        string version2;
        string queryType;
        string word;
        size_t bufferKB = 256;
        int topK = 0;

        for (int i = 1; i < argc; ++i) {
            string arg = argv[i];

            if (arg == "--file") file1 = nextArg(i, argc, argv, arg);
            else if (arg == "--file1") file1 = nextArg(i, argc, argv, arg);
            else if (arg == "--file2") file2 = nextArg(i, argc, argv, arg);
            else if (arg == "--version") version1 = nextArg(i, argc, argv, arg);
            else if (arg == "--version1") version1 = nextArg(i, argc, argv, arg);
            else if (arg == "--version2") version2 = nextArg(i, argc, argv, arg);
            else if (arg == "--buffer") bufferKB = static_cast<size_t>(stoul(nextArg(i, argc, argv, arg)));
            else if (arg == "--query") queryType = nextArg(i, argc, argv, arg);
            else if (arg == "--word") word = nextArg(i, argc, argv, arg);
            else if (arg == "--top") topK = stoi(nextArg(i, argc, argv, arg));
            else throw invalid_argument("Unknown argument: " + arg);
        }

        auto start = chrono::high_resolution_clock::now();

        VersionedWordIndex index;
        unique_ptr<Query> query;

        if (queryType == "word") {
            if (file1.empty() || version1.empty() || word.empty()) {
                throw invalid_argument("Word query requires --file, --version, and --word");
            }
            index.indexFile(file1, version1, bufferKB);
            query = make_unique<WordQuery>(version1, word);
        } else if (queryType == "top") {
            if (file1.empty() || version1.empty() || topK <= 0) {
                throw invalid_argument("Top-K query requires --file, --version, and --top <positive integer>");
            }
            index.indexFile(file1, version1, bufferKB);
            query = make_unique<TopKQuery>(version1, topK);
        } else if (queryType == "diff") {
            if (file1.empty() || file2.empty() || version1.empty() || version2.empty() || word.empty()) {
                throw invalid_argument("Diff query requires --file1, --version1, --file2, --version2, and --word");
            }
            index.indexFile(file1, version1, bufferKB);
            index.indexFile(file2, version2, bufferKB);
            query = make_unique<DiffQuery>(version1, version2, word);
        } else {
            throw invalid_argument("Invalid or missing query type. Use word, top, or diff.");
        }

        query->execute(index);

        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double> duration = end - start;

        cout << "Buffer size: " << bufferKB << " KB\n";
        cout << "Execution time: " << duration.count() << " seconds\n";
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << '\n';
        printUsage();
        return 1;
    }

    return 0;
}
