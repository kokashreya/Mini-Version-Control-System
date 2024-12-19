## Functionalities Implemented

### 1. Initializing repository

Command to execute: ./mygit init

#### Decription: Initializes a new repository .mygit which is a hidden directory

#### Working Procedure:

- It creates the following files and directories inside it:
  - objects
  - refs
    - heads
      - master
  - config
  - HEAD
  - index

### 2. Hash object

Command to execute: ./mygit hash-object test.txt (or) ./mygit hash-object [-w] test.txt

#### Description: Returns the hash value of an object and optionally writes the compressed object to .mygit/objects

#### Working Procedure:

- The conents of the file are read into a buffer.
- SHA1 hash of the file data is calculated and the object is compressed.
- It is written to .mygit/objects if the argument [-w] is provided.
- Otherwise, the SHA1 hash of the file is printed to the console.

### 3. Cat file

Command to execute: ./mygit cat-file <flag> <file-sha>

#### Description: Reads the object contents from the hash value and prints its contents, type or size, depending on the flag given

#### Working Procedure:

- The object is decompressed and read from the hash value provided in argument, by using the 'readObject' function.
- The type of the file is found by parsing through the file contents and looking for the word "blob" or "tree".
- Depending on the flag provided, the file contents, file size or file type are printed to the console.

### 4. Write tree

Command to execute: ./mygit write-tree

#### Description: Prints the hash value of the current working directory tree and calls a function to write the tree object

#### Working Procedure:

- The current working directory path is passed to a function 'createTreeObj'.
- It iterates through all the files and directories recursively and computes each of its hash values.
- The hash value for the entire tree contents is calculated and compressed.
- Finally, the tree object is written to .mygit/objects.

### 5. List tree

Command to execute: ./mygit ls-tree <tree-sha> (or) ./mygit ls-tree [--name-only] <tree-sha>

#### Description: Reads the tree contents from its hash value and prints it

#### Working Procedure:

- The tree contents are read from the hash value provided in the arguments, by using the 'readObject' function.
- Only the names of the objects inside the tree are printed if [--name-only] is used, otherwise the entire tree contents are printed to the console.

### 6. Add files

Command to execute: ./mygit add . (or) ./mygit add file1.txt file2.txt

#### Description: Adds files to the staging area (index) and creates objects for them if not already created

#### Working Procedure:

- The files to be staged are pushed into a vector called files.
- For each file in the vector, the 'stageFile' function is called, which updates an unordered map 'indexFiles' with its mode, type, hash and filename.
- 'updateIndex' function is called, which writes each entry of the unordered map into the index file.

### 7. Commit changes

Command to execute: ./mygit commit (or) ./mygit commit -m "Commit message"

#### Description: Creates a commit object if there are any staged files in index

#### Working Procedure:

- It checks if the index file contains any stages files.
- It retrieves the parent commit hash value using 'parentCommit' function and obtains the parents commit tree hash from it using 'prevTree' function. This ensures that every commit contains a snapshot of files present in previous commits that were unchanged.
- The staged files in index are stored in a set.
- A commit tree is created and stored by combining files from the parent tree, excluding the currently staged files.
- Staged files are then added to the tree data.
- Index file is erased after committing.
- Timestamp is calculated using chrono::system_clock.
- Committed information is stored and hashed.
- The commit hash is compressed and stored as a commit object in .mygit/objects.
- Finally, the refs/heads/master is updated to point to the new commit hash value.

### 8. Log command

Command to execute: ./mygit log

#### Description: Displays commit history from latest to oldest

#### Working Procedure:

- It checks the refs/heads/master for any previous commit hash value.
- If there exists a previous commit, it reads that commit tree using the hash value and prints all the commit details to the console.
- It repeats this by taking the parent hash in each iteration until it is empty.

### 9. Checkout command

Command to execute: ./mygit checkout <commit-sha>

#### Description: Checks out a specific commit, restoring the state of the project as it was at that commit

#### Working Procedure:

- It reads the commit hash given as argument and retrives its tree hash.
- It clears the entire working directory except the .mygit folder.
- It calls the 'prevState' function which reads the tree contents from the tree hash. If it is a blob object, it creates the file using its hash value and if it is a tree, it creates the directory and recursively calls the function to create all the files inside it.
- Finally, it updates the refs/heads/master to contain the new commit hash.

## Important libraries used

- #include <openssl/sha.h> for SHA1 caluclation
- #include <zlib.h> for compression and decompression

## How to compile the code

make main
