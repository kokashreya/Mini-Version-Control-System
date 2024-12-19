#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <openssl/sha.h>
#include <vector>
#include <unordered_map>
#include <set>
#include <zlib.h>
using namespace std;
using namespace std::filesystem;

//creates .mygit directory
void init(){
    if(exists(".mygit")){
        cout << ".mygit already exists\n";
        return;
    }
    try{
        create_directory(".mygit");
        create_directory(".mygit/objects");
        create_directory(".mygit/refs");
        create_directory(".mygit/refs/heads");
        ofstream headFile(".mygit/HEAD");
        if(headFile.is_open()){
            headFile << "ref: refs/heads/master\n";
            headFile.close();
        }
        else{
            cout << "Failed to create HEAD file\n";
            exit(0);
        }
        ofstream masterFile(".mygit/refs/heads/master");
        masterFile.close();
        
        ofstream configFile(".mygit/config");
        configFile.close();

        ofstream indexFile(".mygit/index");
        indexFile.close();

        cout << "Initialized mygit directory\n";
    }
    catch(const exception& e){
        cerr << "Failed to initialize repository: " << e.what() << "\n";
    }
}

//calculates SHA1 hash
string SHA1(const string& data){
    SHA_CTX sha1;
    SHA1_Init(&sha1);
    SHA1_Update(&sha1, data.c_str(), data.size());
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha1);
    
    stringstream ss;
    for(int i=0; i<SHA_DIGEST_LENGTH; i++){
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

//compresses objects
string compressFile(const string& fileData){
    string compressedData;
    compressedData.resize(compressBound(fileData.size()));
    uLongf compressedSize = compressedData.size();
    if(compress(reinterpret_cast<Bytef *>(&compressedData[0]), &compressedSize, reinterpret_cast<const Bytef *>(fileData.c_str()), fileData.size()) != Z_OK){
        cout << "Could not compress the file\n";
        exit(0);
    }
    compressedData.resize(compressedSize);
    return compressedData;
}

//decompresses objects
string decompressFile(const string& compressedData){
    uLongf decompressedSize = compressedData.size() * 2;
    vector<unsigned char> buffer(decompressedSize);
    int status = uncompress(buffer.data(), &decompressedSize, reinterpret_cast<const Bytef *>(compressedData.c_str()), compressedData.size());
    
    while(status == Z_BUF_ERROR){
        buffer.resize(buffer.size() * 2);
        decompressedSize = buffer.size();
        status = uncompress(buffer.data(), &decompressedSize, reinterpret_cast<const Bytef *>(compressedData.c_str()), compressedData.size());
    }
    if(status != Z_OK){
        cout << "Decompression failed\n";
        exit(0);
    }
    return string(buffer.begin(), buffer.begin() + decompressedSize);
}

//writing compressed objects to .mygit/objects
void writeObject(const string& hash, const string& compressedFile){
    string dir = ".mygit/objects/objects" + hash.substr(0,2);
    string file = hash.substr(2);

    if(!exists(dir)){
        create_directory(dir);
    }
    string fullFilePath = dir + "/" + file;
    int fd = open(fullFilePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if(fd < 0){
        cout << "Cannot open file for writing\n";
        perror("open");
        exit(0);
    }
    ssize_t bytesWritten = write(fd, compressedFile.c_str(), compressedFile.size());
    if(bytesWritten < 0){
        cout << "Could not write to file\n";
    }
    close(fd);
}

//returning the hash value of an object and optionally writing the compressed object to .mygit/objects
string handleBlob(const string& filePath, bool store){
    string fileHash;
    if(store == true){
        ifstream in(filePath);
        if(!in.is_open()){
            cout << "Cannot open file" << "\n";
            exit(0);
        }
        ostringstream buffer;
        buffer << in.rdbuf();
        in.close();
        string fileData = buffer.str();
        fileHash = SHA1(fileData);
        string blobPath = ".mygit/objects/objects" + fileHash.substr(0,2) + "/" +fileHash.substr(2);
        if(!exists(blobPath)){
            string compressedFile = compressFile(fileData);
            writeObject(fileHash, compressedFile);
        }
    }
    else{
        ifstream in(filePath);
        if(!in.is_open()){
            cout << "Cannot open file" << "\n";
            exit(0);
        }
        ostringstream buffer;
        buffer << in.rdbuf();
        in.close();
        string fileData = buffer.str();
        fileHash = SHA1(fileData);
        string compressedFile = compressFile(fileData);
    }
    return fileHash;
}

//printing the hash of an object
void hashObject(const string& file, bool store){
    string hash = handleBlob(file, store);

    if(store){
        cout << hash << "\n";
        cout << "Stored the object\n";
    }
    else{
        cout << hash << "\n";
    }
}

//creating a tree object of the current working directory and returning its hash value
string createTreeObj(path directoryPath){
    ostringstream treeContent;
    for(const auto &entry: directory_iterator(directoryPath)){
        if(is_regular_file(entry)){
            string fileHash = handleBlob(entry.path().string(), false);
            treeContent << "100644 blob " << fileHash << " " << entry.path().filename().string() << "\n";
        }
        else if(is_directory(entry) && entry.path().filename() != ".mygit"){
            string treeHash = createTreeObj(entry.path());
            if(!treeHash.empty()){
                treeContent << "040000 tree " << treeHash << " " << entry.path().filename().string() << "\n";
            }   
        }
    }
    string treeData = treeContent.str();
    string treeHash = SHA1(treeData);
    string treePath = ".mygit/objects/objects" + treeHash.substr(0,2) + "/" + treeHash.substr(2);
    if(!exists(treePath)){
        string compressedFile = compressFile(treeData);
        writeObject(treeHash, compressedFile);
    }
    return treeHash;
}

//printing the hash value of the current working directory tree and calling a function to write the tree object
string writeTree(){
    if(!exists(".mygit")){
        cout << ".mygit does not exist\n";
        exit(0);
    }
    cout << current_path() <<endl;
    string treeHash = createTreeObj(current_path());
    cout << treeHash << "\n";
    return treeHash;
}

//reading object contents from its hash value upon decompression
string readObject(const string& hash){
    string dir = ".mygit/objects/objects" + hash.substr(0,2);
    string file = hash.substr(2);

    string fullFilePath = dir + "/" + file;
    if(!exists(fullFilePath)){
        cout << "Object not found\n";
        exit(0);
    }
    ifstream in(fullFilePath, ios::binary);
    ostringstream buffer;
    buffer << in.rdbuf();
    string compressedData = buffer.str();
    in.close();
    return decompressFile(compressedData);
}

//returns the type of an object by reading the file contents
string findType(const string& fileData){
    if(fileData.find("blob") != string::npos || fileData.find("tree") != string::npos){
        return "tree";
    }
    else{
        return "blob";
    }
}

//reads the object contents from the hash value and prints its contents, type or size, depending on the flag given
void catFile(const string& flag, const string& hash){
    if(!exists(".mygit")){
        cout << ".mygit does not exist\n";
        exit(0);
    }
    string fileData = readObject(hash);
    if(fileData.empty()){
        cout << "File is empty\n";
        exit(0);
    }
    string objectType = findType(fileData);
    if(flag == "-p"){
        if(objectType == "blob"){
            cout << fileData << "\n";
        }
        else if(objectType == "tree"){
            cout << fileData << "\n";
        }
    }
    else if(flag == "-s"){
        cout << "File size: " << fileData.size() << "\n";
    }
    else if(flag == "-t"){
        cout << objectType << "\n";
    }
}

//reads the tree contents from its hash value and prints it
void lsTree(const string& hash, bool name){
    string treeData = readObject(hash);
    if(treeData.empty()){
        cout << "Tree is empty\n";
    }
    istringstream treeDataStream(treeData);
    string line;
    while(getline(treeDataStream, line)){
        istringstream linestream(line);
        string mode, type, objecthash, objectname;
        linestream >> mode >> type >> objecthash >> objectname;
        if(name){
            cout << objectname << "\n";
        }
        else{            
            cout << mode << " " << type << " " << objecthash << " " << objectname << "\n";
        }
    }
}

//writing to index file 
void updateIndex(const unordered_map<string, pair<string, pair<string,string>>>& indexFiles){
    ofstream indexFile(".mygit/index", ios::trunc);
    if(!indexFile.is_open()){
        cout << "Cannot write to index file\n";
        exit(0);
    }
    for(auto& [file, entry] : indexFiles){
        indexFile << entry.first << " " << entry.second.first << " " << entry.second.second << " " << file << "\n";
    }
    indexFile.close();
}

//updates the unordered map of file details which are to be added to staging area (index)
void stageFile(const string& file, unordered_map<string, pair<string, pair<string,string>>>& indexFiles){
    string hash = handleBlob(file, true);
    string type;
    string mode = is_directory(file) ? "040000" : "100644";
    if(mode == "100644"){
        type = "blob";
    }
    else{
        type = "tree";
    }
    if(indexFiles.find(file) != indexFiles.end()){
        if(indexFiles[file].second.second == hash){
            return;
        }
    }
    indexFiles[file] = {mode, {type, hash}};
}

//returns an unordered map of the file details
unordered_map<string, pair<string, pair<string,string>>> readIndexFiles(){
    unordered_map<string, pair<string, pair<string,string>>> indexFiles;
    ifstream indexFile(".mygit/index");
    if(!indexFile.is_open()){
        return indexFiles;
    }
    string mode, type, hash, path;
    while(indexFile >> mode >> type >> hash >> path){
        indexFiles[path] = {mode, {type, hash}};
    }
    return indexFiles;
}

//creates objects for the files to be added to staging area (index)
void addFiles(const vector<string>& files){
    unordered_map<string, pair<string, pair<string,string>>> indexFiles = readIndexFiles();
    for(const string& file: files){
        if(!exists(file)){
            cout << "File does not exist\n";
            continue;
        }
        if(is_directory(file)){
            for(const auto& entry: recursive_directory_iterator(file)){
                if(entry.path().string().find(".mygit") != string::npos){
                    continue;
                }
                if(is_regular_file(entry)){
                    stageFile(entry.path().string(), indexFiles);
                }
            }
        }
        else{
            stageFile(file, indexFiles);
        }
    }
    updateIndex(indexFiles);
}

//returns the parent commit hash if it exists
string parentCommit(){
    ifstream headFile(".mygit/HEAD");
    if(!headFile.is_open()){
        return "";
    }
    string line;
    getline(headFile, line);
    headFile.close();

    if(line.find("ref:") != string::npos){
        string path = ".mygit/" + line.substr(5);
        ifstream refFile(path);
        if(refFile.is_open()){
            string commitHash;
            getline(refFile, commitHash);
            refFile.close();
            return commitHash;
        }
    }
    return line;
}

//returns the parent tree hash from the parent commit hash
string prevTree(const string& parentHash){
    if(parentHash.empty()){
        return "";
    }
    string parentTreeData = readObject(parentHash);
    istringstream parentDataStream(parentTreeData);
    string line, treeHash;
    while(getline(parentDataStream, line)){
        if(line.find("Tree:") == 0){
            treeHash = line.substr(6);
            break;
        }
    }
    return treeHash;
}

//creates a commit tree with the contents from parent tree excluding the staged index files
void createCommitTree(const string& prevTreeHash, ostringstream& treeData, set<string>& stagedFiles){
    if(!prevTreeHash.empty()){
        string prevTreeData = readObject(prevTreeHash);
        istringstream prevTreeStream(prevTreeData);
        string line;
        while(getline(prevTreeStream, line)){
            istringstream lineStream(line);
            string mode, type, hash, path;
            lineStream >> mode >> type >> hash >> path;
            if(stagedFiles.find(path) == stagedFiles.end()){
                treeData << mode << " " << type << " " << hash << " " << path << "\n";
            }
        }
    }
}

//creates a commit object if there are any staged files in index
void commit(const string& message){
    ifstream indexFile(".mygit/index");
    if(!indexFile.is_open() || indexFile.peek() == ifstream::traits_type::eof()){
        cout << "No staged files/changes\n";
        exit(0);
    }

    //retrieves parent tree contents
    string parentHash = parentCommit();
    string prevTreeHash = prevTree(parentHash);
    
    set<string> stagedFiles;
    ostringstream indexData;

    string mode, type;
    string filePath;
    string fileHash;

    //stores the staged index files in a set
    while(indexFile >> mode >> type >> fileHash >> filePath){
        stagedFiles.insert(filePath);
        indexData << mode << " " << type << " " << fileHash << " " << filePath << "\n";
    }

    ostringstream treeData;

    //creates commit tree by combining files from parent tree
    createCommitTree(prevTreeHash, treeData, stagedFiles);
    //adds the staged index files to the tree
    treeData << indexData.str();

    //erases the index file after committing
    ofstream eraseIndexFile(".mygit/index", ios::out);
    eraseIndexFile.close();

    string treeEntry = treeData.str();
    string treeHash = SHA1(treeEntry);
    string compressedTree = compressFile(treeEntry);
    writeObject(treeHash, compressedTree);

    auto currTime = chrono::system_clock::now();
    time_t timestamp = chrono::system_clock::to_time_t(currTime);
    
    ostringstream commitData;
    commitData << "Tree: " << treeHash << "\n";
    if(!parentHash.empty()){
        commitData << "Parent: " << parentHash << "\n";
    }
    commitData << "Commit message: " << message << "\n";
    commitData << "Date: " << ctime(&timestamp);

    string commitEntry = commitData.str();
    string commitHash = SHA1(commitEntry);
    string compressedEntry = compressFile(commitEntry);
    writeObject(commitHash, compressedEntry);

    //updates refs/heads/master to contain the new commit hash
    ifstream headFile(".mygit/HEAD");
    if(headFile.is_open()){
        string line;
        getline(headFile, line);
        headFile.close();
        if(line.find("ref:") != string::npos){
            string path = ".mygit/" + line.substr(5);
            ofstream refFile(path);
            if(refFile.is_open()){
                refFile << commitHash << "\n";
                refFile.close();
            }
        }
        else{
            ofstream out(".mygit/HEAD");
            out << commitHash << "\n";
            out.close();
        }
    }
}

/*checks the refs/heads/master file for any previous commit hash
  if there exists a previous commit, it reads that commit tree using the hash value and prints all the commit details
  it repeats this by taking the parent hash in each iteration until it is empty*/
void log(){
    ifstream headFile(".mygit/HEAD");
    if(!headFile.is_open()){
        cout << "No commit history\n";
        return;
    }
    string lastCommit;
    getline(headFile, lastCommit);
    headFile.close();
    if(lastCommit.find("ref:") != string::npos){
        string path = ".mygit/" + lastCommit.substr(5);
        ifstream refFile(path);
        if(refFile.is_open()){
            getline(refFile, lastCommit);
            refFile.close();
        }
    }
    string currCommit = lastCommit;
    while(!currCommit.empty()){
        string commitData = readObject(currCommit);
        if(commitData.empty()){
            cout << "Commit info not found\n";
            break;
        }
        cout << "SHA: " << currCommit << "\n";
        istringstream commit(commitData);
        string line;
        string parentHash;
        while(getline(commit, line)){
            if(line.find("Parent:") == 0){
                parentHash = line.substr(8);
                cout << "Parent SHA: " << line.substr(8) << "\n";
            }
            else if(line.find("Commit message:") == 0){
                cout << "Commit message: " << line.substr(15) << "\n";
            }
            else if(line.find("Date:") == 0){
                cout << "Date: " << line.substr(6) << "\n";
            }
        }
        cout << "Author: Shreya Koka <shreya.koka@students.iiit.ac.in>\n"; 
        cout << "\n";
        currCommit = parentHash;
    }
}

/*reads the tree contents from the hash value
  if it is a blob object, it creates the file using its hash value
  if it is a tree, it creates the directory and recursively calls the function to create all the files inside it*/
void prevState(const string& treeHash, const string& currPath){
    string treeData = readObject(treeHash);
    istringstream treeStream(treeData);
    string line;
    while(getline(treeStream, line)){
        istringstream lineStream(line);
        string mode, type, hash, name;
        lineStream >> mode >> type >> hash >> name;
        if(type == "blob"){
            string fileData = readObject(hash);
            ofstream out(currPath + "/" + name);
            out << fileData;
            out.close();
        }
        else if(type == "tree"){
            create_directory(currPath + "/" + name);
            prevState(hash, currPath + "/" + name);
        }
    }
}

/*reads the commit hash given as argument and retrives its tree hash
  clears the entire working directory except the .mygit folder
  updates the refs/heads/master to contain the new commit hash*/
void checkout(const string& commitHash){
    string commitTree = readObject(commitHash);
    istringstream commitTreeData(commitTree);
    string line;
    string treeHash;
    while(getline(commitTreeData, line)){
        if(line.find("Tree:") == 0){
            treeHash = line.substr(6);
            break;
        }
    }
    if(treeHash.empty()){
        cout << "No previous commits\n";
        exit(0);
    }
    for(const auto& entry : directory_iterator(".")){
        if(entry.path().filename() != ".mygit"){
            remove_all(entry.path());
        }
    }
    prevState(treeHash, ".");

    ifstream headFile(".mygit/HEAD");
    if(headFile.is_open()){
        string line;
        getline(headFile, line);
        headFile.close();
        if(line.find("ref:") != string::npos){
            string path = ".mygit/" + line.substr(5);
            ofstream refFile(path);
            if(refFile.is_open()){
                refFile << commitHash << "\n";
                refFile.close();
            }
        }
        else{
            ofstream out(".mygit/HEAD");
            out << commitHash << "\n";
            out.close();
        }
    }
}

int main(int argc, char* argv[]){
    bool store = false;
    bool name = false;
    string cmd = argv[1];
    if(cmd == "init"){
        if(argc != 2){
            cout << "Wrong command format\n";
            exit(0);
        }
        init();
    }
    else if(cmd == "hash-object"){
        string arg = argv[2];
        if(arg == "[-w]"){
            store = true;
            string file = argv[3];
            hashObject(file, store);
        }
        else{
            string file = argv[2];
            hashObject(file, store);
        }
    }
    else if(cmd == "cat-file"){
        string flag = argv[2];
        string hash = argv[3];
        catFile(flag, hash);
    }
    else if(cmd == "write-tree"){
        string treeHash = writeTree();
    }
    else if(cmd == "ls-tree"){
        string isName = argv[2];
        if(isName == "[--name-only]"){
            name = true;
            string hash = argv[3];
            lsTree(hash, name);
        }
        else{
            string hash = argv[2];
            lsTree(hash, name);
        }
    }
    else if(cmd == "add"){
        vector<string> files;
        string arg = argv[2];
        if(arg == "."){
            files.push_back(".");
        }
        else{
            for(int i=2; i<argc; i++){
                files.push_back("./" + string(argv[i]));
            }
        }
        addFiles(files);
    }
    else if(cmd == "commit"){
        string message = "";
        if(argc == 4 && string(argv[2]) == "-m"){
            message = argv[3];
            commit(message);
        }
        else if(argc == 2){
            string message = "Default commit message";
            commit(message);
        }
        else{
            cout << "Invalid command format\n";
        }
    }
    else if(cmd == "log"){
        log();
    }
    else if(cmd == "checkout"){
        string commitHash = argv[2];
        checkout(commitHash);
    }
}