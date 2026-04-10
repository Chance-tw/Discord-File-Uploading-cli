//save bot token to DISCORD_TOKEN env var with 'export DISCORD_TOKEN="token"'
//save discord channel ID to DISCORD_CHANNEL env var with 'export DISCORD_CHANNEL="uint64 channel id"'
//channel ID in file_info struct
//compile with: g++ discord_file_upload.cpp -o discord_file_upload -ldpp -lcurl
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <dpp/dpp.h>
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <atomic>

struct file_info{
    dpp::snowflake channel_id; //channel ID for bot manage
    std::string name; //Original file name/path
    std::string key; //XOR decryption key
    std::vector<uint64_t> msg_ids; //list of the message IDs and their index numbers
};

//general purpose
bool filecheck(std::string file_path);
bool readandvalidate(file_info &file, bool download);

//upload
void encrypt(file_info &upfile_info);
void split(const size_t &TEN_MB, file_info &upfile_info);

//download
void disc_down(file_info &downfile_info);
void decrypt(file_info &downfile_info);

int main(){
    srand(time(nullptr)); //seed rand
    const size_t TEN_MB = 10 * 1024 * 1024; //max file upload size
    std::string action;
    std::vector<std::string> file_names;
    std::vector<std::string> keys;

    if (filecheck("discord_files.txt")){ //read in already uploaded file names since upload checks to makes sure a file isnt already uploaded and would causes naming conflicts, and download since of course
        std::ifstream disc_files_check("discord_files.txt");
        if (!disc_files_check){
            std::cout<<"Error reading discord_files.txt!\n";
            exit(0);
        }
    
        //Get names of already uploaded files
        std::string line;
        bool save_next=false;
        bool get_key=false;
        while (getline(disc_files_check, line)){
            if (get_key){
                keys.push_back(line);
                get_key=false;
            }

            if (save_next){ //after file name is key so next line save key
                file_names.push_back(line);
                get_key=true;
                save_next=false;
            }

            if (line.empty()){ //if is empty next is a file name
                save_next=true;
            }
        }

        disc_files_check.close();
    }

    while (true){
        std::cout<<"Upload or download (u/d): ";
        getline(std::cin, action);
        transform(action.begin(), action.end(), action.begin(), ::tolower);
        if (action=="u" || action=="up" || action=="upload"){
            file_info upfile_info;

            char* channel=getenv("DISCORD_CHANNEL");
            if (channel==NULL){
                std::cout<<"DISCORD_CHANNEL environment variable not set\n";
                exit(0);
            }
            upfile_info.channel_id=std::stoull(channel); //channel ID for bot

            while (true){ //loop until valid file path given and its not already uploaded
                std::cout<<"Path to file: ";
                getline(std::cin, upfile_info.name);

                if (!filecheck(upfile_info.name)){ //if its a valid file
                    std::cout<<"Invalid input!\n";
                    continue;
                }

                if (file_names.size()==0){ //if no uploads have been made
                    break;
                }

                for (int i=0; i<file_names.size(); i++){ //check if file has already been uploaded and would cause naming conflicts if done again
                    if (upfile_info.name==file_names[i]){
                        std::cout<<"File already uploaded\n";
                        continue;
                    }
                }

                break;
            }

            //encrypt, split, and upload
            encrypt(upfile_info);
            split(TEN_MB, upfile_info);

            std::ofstream disc_files("discord_files.txt", std::ios::app); //append the uploaded info to the index file
            if (!disc_files){
                std::cout<<"Error writing to discord_files.txt\n";
                exit(0);
            }

            disc_files<<std::endl; //blank space for reading later
            disc_files<<upfile_info.name<<std::endl;
            disc_files<<upfile_info.key<<std::endl;
            for (int i=upfile_info.msg_ids.size()-1; i>=0; i--){
                disc_files<<upfile_info.msg_ids[i]<<std::endl;
            }
            disc_files.close();

            if (readandvalidate(upfile_info, false)){
                std::cout<<"Upload successful\n";
                break;
            }
        }//if upload
        else if (action=="d" || action=="down" || action=="download"){
            if (file_names.size()==0){ //if no uploads have been made
                std::cout<<"No uploaded files found\n";
                continue;
            }
            
            file_info downfile_info;

            char* channel=getenv("DISCORD_CHANNEL");
            if (channel==NULL){
                std::cout<<"DISCORD_CHANNEL environment variable not set\n";
                exit(0);
            }
            downfile_info.channel_id=std::stoull(channel); //channel ID for bot

            std::cout<<"*****Uploaded files*****\n";
            for (size_t i=0; i<file_names.size(); i++){
                std::cout<<i+1<<".  "<<file_names[i]<<'\n';
            }

            //get the file num of what to upload
            std::string str_num;
            int num;
            while (true){
                std::cout<<"File number to download: ";
                getline(std::cin, str_num);
                try{
                    num=stoi(str_num);
                    break;
                }
                catch (std::invalid_argument& e){
                    std::cout<<"Invalid input!\n";
                }
                catch (std::out_of_range& e){
                    std::cout<<"Invalid input!\n";
                }
            }

            //populate struct with the wanted file info
            downfile_info.name=file_names[num-1];
            downfile_info.key=keys[num-1];
            readandvalidate(downfile_info, true);

            disc_down(downfile_info);

            break;
        }//if download
        else{
            std::cout<<"Invalid input!\n";
            continue;
        }
    }//while action not valid
    
    return 0;
}

bool filecheck(std::string file_path){
    std::ifstream file_check(file_path);
    return file_check.good();
}

bool readandvalidate(file_info &file, bool download){
    std::ifstream disc_files_check("discord_files.txt");
    if (!disc_files_check){
        std::cout<<"Error reading discord_files.txt\n";
        exit(0);
    }

    std::string line;
    bool savenext=false;
    std::vector<uint64_t> msg_ids_and_index;
    while (getline(disc_files_check, line)){
        if (line.empty() && savenext){ //if the line is empty and after the key that marks the start of new file index block
            break;
        }

        if (savenext){
            msg_ids_and_index.push_back(stol(line));
        }

        if (line==file.key){
            savenext=true;
        }
    }

    if (msg_ids_and_index.size()%2!=0){ //should be even as msg id and index always come as pairs
        std::cout<<"Message data in discord_files.txt is corrupt!\n";
        exit(0);
    }

    if (download){
        std::vector<uint64_t> sorted_msg_ids(msg_ids_and_index.size()/2);
        for (int i=0; i<msg_ids_and_index.size(); i++){
            if (i%2==0){
                if (msg_ids_and_index[i]<msg_ids_and_index[i+1]){
                    sorted_msg_ids[msg_ids_and_index[i]]=msg_ids_and_index[i+1];
                }
                else{
                    sorted_msg_ids[msg_ids_and_index[i+1]]=msg_ids_and_index[i];
                }
            }
        }
        file.msg_ids=sorted_msg_ids;
    }

    return true; //exits whole program if corrupt, so return true if not exited
}

void encrypt(file_info &upfile_info){
    //create key
    char key_element;
    std::string key;
    if (upfile_info.key.empty()){
        for (int i=0; i<30; i++){ //30 random chars
            key_element=33+rand()%(126-33+1);
            key+=key_element;
        }
    }
    upfile_info.key=key;

    //open original file
    std::ifstream og(upfile_info.name, std::ios::binary);
    if (!og){
        std::cout<<"Error opening "<<upfile_info.name<<'\n';
        exit(0);
    }

    //create the encrypted version
    std::string encrypted_path=upfile_info.name+"_encrypted";
    std::ofstream encrypted(encrypted_path, std::ios::binary);
    if (!encrypted){
        std::cout<<"Error creating encrypted file:"<<encrypted_path<<'\n';
        exit(0);
    }

    //XOR encrypt the original
    char buffer;
    size_t i = 0;
    while (og.get(buffer)) {
        buffer ^= key[i++ % key.size()];
        encrypted.put(buffer);
    }
}

void split(const size_t &TEN_MB, file_info &upfile_info){
    std::string up_root="discord_upload_"; //root for the name of the smaller file chunks to upload
    std::vector<std::string> temp_files; //for a list of these files to remove after uploading is complete

    std::ifstream upfile(upfile_info.name+"_encrypted", std::ios::binary); //open file to upload

    if (!upfile){
        std::cout<<"Error opening "<<upfile_info.name<<"_encrypted"<<'\n';
        exit(0);
    }

    upfile.seekg(0, std::ios::end); //finds EOF
    size_t size=upfile.tellg(); //saves how much it read until it found EOF to size
    upfile.seekg(0, std::ios::beg); //resets reading position to the beginning

    const char* token=getenv("DISCORD_TOKEN");
    if (token==NULL){
        std::cout<<"DISCORD_TOKEN environment variable not set\n";
        exit(0);
    }

    dpp::cluster bot(token); //defines a bot cluster/lambda using the token
    //bot.on_log(dpp::utility::cout_logger()); //for bot logs displayed in terminal

    bot.on_ready([&bot, &upfile_info, TEN_MB, &size, &upfile, up_root, &temp_files](const dpp::ready_t& event){
        int index=0; //index numbering for the files getting uploaded

        std::mutex mtxlock; //creates a mutex to lock the threads
        std::condition_variable cvnotifier; //creates a condition var so it can notify when to switch the mutex ownership
        std::atomic<bool> uploaded{false}; //bool that gets switched for the condition var to notify

        while (size>0){ //loops until no data left 
            size_t chunk_size=std::min(TEN_MB, size); //takes either a chunk of ten MB or whatver is left of size
            std::vector<char> file_chunk(chunk_size); //initializes a vector to the chunk size being taken

            upfile.read(file_chunk.data(), chunk_size); //read the binary data of the upfile into the file_chunk vector
            if (!upfile){
                std::cout<<"Error reading "<<upfile_info.name+"_encrypted"<<'\n';
                exit(0);
            }

            std::string upfile_chunk=up_root+std::to_string(index); //sets the file name with its index number
            std::ofstream file_piece(upfile_chunk, std::ios::binary | std::ios::trunc); //truncating mode in case file already existed and held data
            if (!file_piece){
                std::cout<<"Error opening a discord_upload_(index) file\n";
                exit(0);
            }

            file_piece.write(file_chunk.data(), chunk_size); //writes the vector holding the data to the file
            file_piece.close();

            temp_files.push_back(upfile_chunk); //adds the discord_upload_(index) file to the temp files

            upfile_info.msg_ids.push_back(index); //adds a messages index to proceed it in msg_ids

            std::cout<<"Uploading: "<<upfile_chunk<<'\n';
            uploaded=false; //sets uploaded back to false on each iteration

            const dpp::snowflake channel_id=upfile_info.channel_id; //set channel id to upload to

            dpp::message msg(channel_id, ""); //creates a message to channel containing no text
            msg.add_file(upfile_chunk, dpp::utility::read_file(upfile_chunk)); //adds a file to the message

            bot.message_create(msg, [&bot, &upfile_info, &upfile_chunk, &index, &uploaded, &mtxlock, &cvnotifier] (const dpp::confirmation_callback_t& callback){
                if (callback.is_error()){ //if the callback response from discord is an error
                    std::cout<<"Failed to send message\n";
                    exit(0);
                }

                //gets the message id
                dpp::message sent=std::get<dpp::message>(callback.value); //gets the callback object

                std::cout<<"Uploaded: "<<upfile_chunk<<'\n';

                upfile_info.msg_ids.push_back(sent.id); //add the ID from the callback message object to the msg ID list

                //Notify the main thread that upload finished
                {
                    std::lock_guard<std::mutex> lock(mtxlock);
                    uploaded=true;
                }
                cvnotifier.notify_one();
            } /* the callback */ ); //the lambda
            {
                //waits for uploaded to become true before letting the main thread continue
                std::unique_lock<std::mutex> lock(mtxlock);
                cvnotifier.wait(lock, [&uploaded]{return uploaded.load();});
            }
            index++; //increment index
            size-=chunk_size; //subtract the size taken from the total size
        }//while file has data
        bot.shutdown();
    });

    bot.start(dpp::st_wait);

    //removes the temp discord_upload_(index) files
    for (int i=0; i<temp_files.size(); i++){
        remove(temp_files[i].c_str());
    }

    //removes the encrypted version
    remove((upfile_info.name+"_encrypted").c_str());
}

void disc_down(file_info &downfile_info){
    //sets up output file to form the encrypted again
    std::string encrypted_path=downfile_info.name+"_encrypted_download";
    std::ofstream encrypted(encrypted_path, std::ios::binary | std::ios::app); //append mode since little pieces getting added to it
    if (!encrypted){
        std::cout<<"Failed to create output file: "<<encrypted_path<<'\n';
        exit(0);
    }

    std::vector<std::string> temp_files; //for the temp files to be removed after appending

    const char* token=getenv("DISCORD_TOKEN");
    if (token==NULL){
        std::cout<<"DISCORD_TOKEN environment variable not set\n";
        exit(0);
    }

    dpp::cluster bot(token); //defines a bot cluster/lambda using the token
    //bot.on_log(dpp::utility::cout_logger()); //for bot logs

    bool cont=false; //for busy loop to stop until file is downloaded

    bot.on_ready([&bot, &downfile_info, &encrypted, &cont, &temp_files](const dpp::ready_t&){
        const dpp::snowflake channel_id=downfile_info.channel_id; //set channel id to download from
        for (int i=0; i<downfile_info.msg_ids.size(); i++){
            cont=false; //set cont back to false on each iteration

            dpp::snowflake message_id=downfile_info.msg_ids[i]; //desired message to be downloaded

            bot.message_get(message_id, channel_id, [&bot, &encrypted, &downfile_info, i, &cont, &temp_files](const dpp::confirmation_callback_t& event){
                    //find message and its info
                    auto msg=std::get<dpp::message>(event.value);
                    auto attach = msg.attachments[0];
                    std::string file_name=attach.filename; //bots retrieved file name
                    std::string file_url=attach.url; //the url to download the file from

                    std::cout<<"Downloading: "<<file_name<<" from "<<file_url<<'\n';

                    //download the message
                    bot.request(file_url, dpp::m_get, [file_name, &encrypted, &downfile_info, i, &cont, &temp_files](const dpp::http_request_completion_t& r){
                        if (r.status==200){ //200==successful
                            //writes thr message body to the file
                            std::ofstream out(file_name, std::ios::binary);
                            out<<r.body;
                            out.close();
                            std::cout<<"Successfully downloaded "<<file_name<<'\n';

                            std::ifstream downloaded(file_name, std::ios::binary);
                            if (!downloaded){
                                std::cout<<"Error opening downloaded file: "<<file_name<<'\n';
                                    exit(0);
                            }

                            encrypted<<downloaded.rdbuf(); //adds the just downloaded to the main file
                            downloaded.close();

                            temp_files.push_back(file_name); //adds temp file name to be removed after

                            cont=true; //sets cont to true so the busy loop will let the download move on to the next download

                            //close the main file once the last index is retrieved then decrypts it
                            if (i==downfile_info.msg_ids.size()-1){
                                encrypted.close();
                                decrypt(downfile_info);
                            }
                        } 
                        else{ //https status not 200 so it failed
                            std::cout<<"Download failed on "<<file_name<<'\n';
                            exit(0);
                        }
                    });
                }
            );

            while(!cont){ //waits for file to be written to disk
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        bot.shutdown();
    });

    bot.start(dpp::st_wait);

    //mainly removed not just after download since cont only waits for file to be downloaded and idk may not finish appending before continuing
    for (int i=0; i<temp_files.size(); i++){
        remove(temp_files[i].c_str());
    }
}

void decrypt(file_info &downfile_info){
    std::ifstream encrypted(downfile_info.name+"_encrypted_download", std::ios::binary);
    if (!encrypted){
        std::cout<<"Error opening "<<downfile_info.name<<"_encrypted_download"<<'\n';
        exit(0);
    }

    std::string decrypted_path=downfile_info.name;
    //ensures if the file is still on disk its not overwritten
    if (filecheck(decrypted_path)){ 
        decrypted_path+="_new";
    }

    std::ofstream decrypted(decrypted_path, std::ios::binary);
    if (!decrypted){
        std::cout<<"Error creating "<<decrypted_path<<'\n';
        exit(0);
    }

    //undoes the simple XOR by applying the same operation as encryption
    char buffer;
    size_t i=0;
    while (encrypted.get(buffer)) {
        buffer ^= downfile_info.key[i++ % downfile_info.key.size()];
        decrypted.put(buffer);
    }

    remove((downfile_info.name+"_encrypted_download").c_str()); //removes the encrypted versin just downloaded

    std::cout<<"Downloaded content at "<<decrypted_path<<'\n';
}