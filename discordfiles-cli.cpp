//save bot token to DISCORD_TOKEN env var with 'export DISCORD_TOKEN="token"'
//save discord channel ID to DISCORD_CHANNEL env var with 'export DISCORD_CHANNEL="uint64 channel id"'
//compile with: g++ discordfiles-cli.cpp -o discordfiles-cli -ldpp -lcurl
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
bool readandvalidate(file_info &file, bool download, std::string &indexfile);

//upload
void encrypt(file_info &upfile_info, bool verbose);
void split(const size_t &TEN_MB, file_info &upfile_info, bool verbose, std::string &indexfile);

//download
void disc_down(file_info &downfile_info, bool verbose, bool difout, std::string &diffile);
void decrypt(file_info &downfile_info, bool verbose, bool difout, std::string &diffile);

int main(int argc, char* argv[]){
    //print the help menu

    if (argc==1 || (argc==2 && (std::string(argv[1])=="-h" || std::string(argv[1])=="--help"))){
        std::cout<<
        "     ____  _                       _   ____  _\n"
        "    |  _ \\(_)___  ___ ___  _ __ __| | |  __|(_)_  ___\n"
        "    | | \\ | / __|/ __/ _ \\| '__/ _  | | |_  | | |/ _ \\\n"
        "    | |_/ | \\__ \\ (_| (_) | | | (_| | |  _| | | |  __/\n"
        "    |____/|_|___/\\___\\___/|_|  \\____| |_|   |_|_|\\___|\n"
        "     _   _                         _ _\n"
        "    | | | |____  _  ___   __ _  __| (_)_ __   ____\n"
        "    | | | |  _ \\| |/ _ \\ / _` |/ _  | | '_ \\ / _  |\n"
        "    | |_| | |_) | | (_) | (_| | (_| | | | | | (_| |\n"
        "     \\___/|  __/|_|\\___/ \\__,_|\\____|_|_| |_|\\__, |\n"
        "          |_|                                 |___/\n";
        /*
         ____  _                       _   ____  _
        |  _ \(_)___  ___ ___  _ __ __| | |  __|(_)_  ___
        | | \ | / __|/ __/ _ \| '__/ _  | | |_  | | |/ _ \
        | |_/ | \__ \ (_| (_) | | | (_| | |  _| | | |  __/
        |____/|_|___/\___\___/|_|  \____| |_|   |_|_|\___|
         _   _                         _ _
        | | | |____  _  ___   __ _  __| (_)_ __   ____
        | | | |  _ \| |/ _ \ / _` |/ _  | | '_ \ / _  |
        | |_| | |_) | | (_) | (_| | (_| | | | | | (_| |
         \___/|  __/|_|\___/ \__,_|\____|_|_| |_|\__, |
              |_|                                 |___/
        */
        
        std::cout<<"-h, --help          print this screen\n";
        std::cout<<'\n';
        std::cout<<"flags stackable with any:\n";
        std::cout<<"-v, --verbose       prints discord bot logs and other information on program activity\n";
        std::cout<<"-l, --list          lists available files to download\n";
        std::cout<<'\n';
        std::cout<<"flags not stackable with eachother:\n";
        std::cout<<"-u, --upload        file to upload, --upload <filepath>\n";
        std::cout<<"-d, --download      file to download, --download <stored_name(printed with --list or -l)>\n";
        std::cout<<"-o, --output        for sending downloaded file to specified path, --output <filepath>\n";
        std::cout<<"                        if path is not specified then it will download to the path in --list\n";
        std::cout<<'\n';
        std::cout<<"Ensure DISCORD_TOKEN and DISCORD_CHANNEL environment variables are set\n";
        return 0;
    }

    bool list=false;
    bool verbose=false;

    bool upload=false;
    bool download=false;
    std::string file;

    bool difout=false;
    std::string diffile;

    bool advancei=false;

    //to do: make sure that the advancing to capture next only happens if it wont go out of range

    //process inputs
    for (int i=1; i<argc; i++){
        if (argv[i][0]=='-' && argv[i][1]=='-'){ //the arg is a full word command like --verbose
            if (std::string(argv[i])=="--verbose"){
                verbose=true;
                continue;
            }
            else if (std::string(argv[i])=="--list"){
                list=true;
                continue;
            }
            else if (std::string(argv[i])=="--upload"){
                upload=true;
                //read next arg and advance i past it
                if ((i+1)<argc){
                    i++;
                    file=argv[i];
                    continue;
                }
                else{
                    std::cout<<"Invalid input: no file path\n";
                    return 0;
                }
            }
            else if (std::string(argv[i])=="--download"){
                download=true;
                //read next arg and advance i past it
                if ((i+1)<argc){
                    i++;
                    file=argv[i];
                    continue;
                }
                else{
                    std::cout<<"Invalid input: no file path\n";
                    return 0;
                }
            }
            else if (std::string(argv[i])=="--output"){
                difout=true;
                //read in nect arg and advance i past it
                if ((i+1)<argc){
                    i++;
                    diffile=argv[i];
                    continue;
                }
                else{
                    std::cout<<"Invalid input: no file path\n";
                    return 0;
                }
            }
            else{
                std::cout<<"Invalid input\n";
                return 0;
            }
        }
        else if (argv[i][0]=='-' && argv[i][1]!='-'){
            if (std::string(argv[i]).length()==1){
                std::cout<<"Invalid input\n";
                return 0;
            }
            std::string arggroup=argv[i];
            for (int j=1; j<arggroup.length(); j++){
                if (argv[i][j]=='v'){
                    verbose=true;
                    continue;
                }
                else if (argv[i][j]=='l'){
                    list=true;
                    continue;
                }
                else if (argv[i][j]=='u'){
                    upload=true;
                    //read next arg and advance i past it
                    if ((i+1)<argc){
                        if (advancei){
                            std::cout<<"Invalid input: unstackable flag already present in group\n";
                            return 0;
                        }
                        advancei=true;
                        file=argv[i+1];
                        continue;
                    }
                    else{
                        std::cout<<"Invalid input: no file path\n";
                        return 0;
                    }
                }
                else if (argv[i][j]=='d'){
                    download=true;
                    //read next arg and advance i past it
                    if ((i+1)<argc){
                        if (advancei){
                            std::cout<<"Invalid input: unstackable flag already present in group\n";
                            return 0;
                        }
                        advancei=true;
                        file=argv[i+1];
                        continue;
                    }
                    else{
                        std::cout<<"Invalid input: no file path\n";
                        return 0;
                    }
                }
                else if (argv[i][j]=='o'){
                    difout=true;
                    //read next arg and advance i past it
                    if ((i+1)<argc){
                        if (advancei){
                            std::cout<<"Invalid input: unstackable flag already present in group\n";
                            return 0;
                        }
                        advancei=true;
                        diffile=argv[i+1];
                        continue;
                    }
                    else{
                        std::cout<<"Invalid input: no file path\n";
                        return 0;
                    }
                }
                else{
                    std::cout<<"Invalid input\n";
                    return 0;
                }
            }
            if (advancei){
                i++;
                advancei=false;
            }
        }
        else{
            std::cout<<"Invalid input\n";
            return 0;
        }
    }

    if (upload && download){
        std::cout<<"Invalid input\n";
        return 0;
    }
    if (upload && difout){
        std::cout<<"Invalid input\n";
        return 0;
    }

    srand(time(nullptr)); //seed rand
    const size_t TEN_MB = 10 * 1024 * 1024; //max file upload size
    std::string action;
    std::vector<std::string> file_names;
    std::vector<std::string> keys;
    int filenum=0;

    //prep for new destination stuff
    char* home=getenv("HOME");
    if (home==NULL){
        std::cout<<"HOME environment variable not set!\n";
        return 0;
    }
    std::string newdirpath=std::string(home)+"/.config/discord_file_uploading";
    std::string indexfilepath=newdirpath+"/discord_files.txt";

    try{
        std::filesystem::create_directories(newdirpath);
    }
    catch(const std::filesystem::filesystem_error& e){ //should catch any errors with writing to the filesystem, but idk how to intentionally cause file system errors
        std::cout<<"Failed to create program directory at "<<newdirpath<<'\n';
        return 0;
    }

    if (filecheck(indexfilepath)){ //read in already uploaded file names since upload checks to makes sure a file isnt already uploaded and would causes naming conflicts, and download since of course
        std::ifstream disc_files_check(indexfilepath);
        if (!disc_files_check){
            std::cout<<"Error reading "<<indexfilepath<<'\n';
            exit(0);
        }
    
        if (verbose){
            std::cout<<"reading info from "<<indexfilepath<<"(operation needed for uploads and downloads)\n";
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

        for (int i=0; i<file_names.size(); i++){ //check if file has already been uploaded and would cause naming conflicts if done again
            if (file==file_names[i]){
                filenum=i+1;
            }
        }
    }

    if (list){
        std::cout<<"uploaded files:\n";
        for (int i=0; i<file_names.size(); i++){ //check if file has already been uploaded and would cause naming conflicts if done again
            std::cout<<file_names[i]<<'\n';
        }
    }

    if (upload){
        file_info upfile_info;

        char* channel=getenv("DISCORD_CHANNEL");
        if (channel==NULL){
            std::cout<<"DISCORD_CHANNEL environment variable not set\n";
            exit(0);
        }

        upfile_info.channel_id=std::stoull(channel); //channel ID for bot
        upfile_info.name=file;
            
        if (!filecheck(upfile_info.name)){ //if its a valid file
            std::cout<<"Invalid input: no file path\n";
            return 0;
        }

        if (filenum>0){
            std::cout<<"File already uploaded\n";
            return 0;
        }
        

        //encrypt, split, and upload
        encrypt(upfile_info, verbose);
        split(TEN_MB, upfile_info, verbose, indexfilepath);
    }//if upload
    else if (download){
        if (file_names.size()==0){ //if no uploads have been made
            std::cout<<"No uploaded files found\n";
            return 0;
        }
            
        file_info downfile_info;

        char* channel=getenv("DISCORD_CHANNEL");
        if (channel==NULL){
            std::cout<<"DISCORD_CHANNEL environment variable not set\n";
            exit(0);
        }

        downfile_info.channel_id=std::stoull(channel); //channel ID for bot
        downfile_info.name=file;
        downfile_info.key=keys[filenum-1];

        readandvalidate(downfile_info, true, indexfilepath);

        disc_down(downfile_info, verbose, difout, diffile);
    }//if download

    return 0;
}

bool filecheck(std::string file_path){
    std::ifstream file_check(file_path);
    if(std::filesystem::is_regular_file(file_path)){
        return file_check.good();
    }
    return false;
}

bool readandvalidate(file_info &file, bool download, std::string &indexfile){
    std::ifstream disc_files_check(indexfile);
    if (!disc_files_check){
        std::cout<<"Error reading "<<indexfile<<'\n';
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
            msg_ids_and_index.push_back(stoull(line));
        }

        if (line==file.key){
            savenext=true;
        }
    }

    if (msg_ids_and_index.size()%2!=0){ //should be even as msg id and index always come as pairs
        std::cout<<"Message data in "<<indexfile<<" is corrupt\n";
        exit(0);
    }

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

    if (sorted_msg_ids.size()==0 || sorted_msg_ids.size()>1000000000000000000){
        std::cout<<"Message data in "<<indexfile<<" is corrupt\n";
        exit(0);
    }
    
    if (download){
        file.msg_ids=sorted_msg_ids;
    }

    return true; //exits whole program if corrupt, so return true if not exited
}

void encrypt(file_info &upfile_info, bool verbose){
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

    if (verbose){
        std::cout<<"key created: "<<key<<'\n';
    }

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

    if (verbose){
        std::cout<<"encrypted: "<<upfile_info.name<<" to: "<<encrypted_path<<'\n';
    }
}

void split(const size_t &TEN_MB, file_info &upfile_info, bool verbose, std::string &indexfile){
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
    if (verbose){
        bot.on_log(dpp::utility::cout_logger()); //for bot logs displayed in terminal
    }

    std::mutex mtxlock; //creates a mutex to lock the threads
    std::condition_variable cvnotifier; //creates a condition var so it can notify when to switch the mutex ownership
    std::atomic<bool> uploaded{false}; //bool that gets switched for the condition var to notify
    std::atomic<bool> done{false}; //for busy loop to know when to shut bot down

    bot.on_ready([&bot, &upfile_info, TEN_MB, &size, &upfile, up_root, &temp_files, &mtxlock, &cvnotifier, &uploaded, &done,verbose](const dpp::ready_t& event){
        int index=0; //index numbering for the files getting uploaded

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

            if (verbose){
                std::cout<<"wrote "<<chunk_size<<" byte chunk to: "<<upfile_chunk<<'\n';
            }

            temp_files.push_back(upfile_chunk); //adds the discord_upload_(index) file to the temp files

            upfile_info.msg_ids.push_back(index); //adds a messages index to proceed it in msg_ids

            if (verbose){
                std::cout<<"Uploading: "<<upfile_chunk<<'\n';
            }
            uploaded=false; //sets uploaded back to false on each iteration

            const dpp::snowflake channel_id=upfile_info.channel_id; //set channel id to upload to

            dpp::message msg(channel_id, ""); //creates a message to channel containing no text
            msg.add_file(upfile_chunk, dpp::utility::read_file(upfile_chunk)); //adds a file to the message

            bot.message_create(msg, [&bot, &upfile_info, &upfile_chunk, &index, &uploaded, &mtxlock, &cvnotifier, verbose] (const dpp::confirmation_callback_t& callback){
                if (callback.is_error()){ //if the callback response from discord is an error
                    std::cout<<"Failed to send message\n";
                    exit(0);
                }

                //gets the message id
                dpp::message sent=std::get<dpp::message>(callback.value); //gets the callback object
                
                if (sent.id==0){
                    std::cout<<"Failed to retrieve message ID in https response\n";
                    exit(0);
                }
                
                if (verbose){
                    std::cout<<"Uploaded: "<<upfile_chunk<<'\n';
                }

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
        done=true;
    });

    bot.start(dpp::st_return); //returns to the thread instead of holding this thread

    while(!done){ //waits for files to be uploaded
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::thread t([&bot](){
            bot.shutdown();
        });
    t.detach();

    if (verbose){
        std::cout<<"removing temp files\n";
    }

    //removes the temp discord_upload_(index) files
    for (int i=0; i<temp_files.size(); i++){
        remove(temp_files[i].c_str());
    }

    //removes the encrypted version
    remove((upfile_info.name+"_encrypted").c_str());

    std::ofstream disc_files(indexfile, std::ios::app); //append the uploaded info to the index file
    if (!disc_files){
        std::cout<<"Error writing to "<<indexfile<<'\n';
        exit(0);
    }

    disc_files<<std::endl; //blank space for reading later
    disc_files<<upfile_info.name<<std::endl;
    disc_files<<upfile_info.key<<std::endl;
    for (int i=upfile_info.msg_ids.size()-1; i>=0; i--){
        disc_files<<upfile_info.msg_ids[i]<<std::endl;
    }
    disc_files.close();

    if (verbose){
        std::cout<<"wrote "<<upfile_info.name<<", key, and msg ID's and indexes to "<<indexfile<<'\n';
    }

    if (readandvalidate(upfile_info, false, indexfile)){
        std::cout<<"Upload successful\n";
        exit(0);
    }
}

void disc_down(file_info &downfile_info, bool verbose, bool difout, std::string &diffile){
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
    if (verbose){
        bot.on_log(dpp::utility::cout_logger()); //for bot logs
    }

    std::atomic<bool> cont{false}; //for busy loop to stop until file is downloaded
    std::atomic<bool> done{false}; //for busy loop to know when to shut bot down

    bot.on_ready([&bot, &downfile_info, &encrypted, &cont, &done, &temp_files, verbose, difout, &diffile](const dpp::ready_t&){
        const dpp::snowflake channel_id=downfile_info.channel_id; //set channel id to download from
        for (int i=0; i<downfile_info.msg_ids.size(); i++){
            cont=false; //set cont back to false on each iteration

            dpp::snowflake message_id=downfile_info.msg_ids[i]; //desired message to be downloaded

            bot.message_get(message_id, channel_id, [&bot, &encrypted, &downfile_info, i, &cont, &done, &temp_files, verbose, difout, &diffile](const dpp::confirmation_callback_t& event){
                    //find message and its info
                    auto msg=std::get<dpp::message>(event.value);
                    auto attach = msg.attachments[0];
                    std::string file_name=attach.filename; //bots retrieved file name
                    std::string file_url=attach.url; //the url to download the file from

                    if (verbose){
                        std::cout<<"Downloading: "<<file_name<<" from "<<file_url<<'\n';
                    }

                    //download the message
                    bot.request(file_url, dpp::m_get, [file_name, &encrypted, &downfile_info, i, &cont, &done, &temp_files, verbose, difout, &diffile](const dpp::http_request_completion_t& r){
                        if (r.status==200){ //200==successful
                            //writes thr message body to the file
                            std::ofstream out(file_name, std::ios::binary);
                            out<<r.body;
                            out.close();
                            if (verbose){
                                std::cout<<"Successfully downloaded "<<file_name<<'\n';
                            }

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
                                decrypt(downfile_info, verbose, difout, diffile);
                                done=true;
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
    });

    bot.start(dpp::st_return); //returns to the thread instead of holding this thread

    while(!done){ //waits for files to be downloaded
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    bot.shutdown();

    if (verbose){
        std::cout<<"removing temp files\n";
    }

    //mainly removed not just after download since cont only waits for file to be downloaded and idk may not finish appending before continuing
    for (int i=0; i<temp_files.size(); i++){
        remove(temp_files[i].c_str());
    }

    exit(0);
}

void decrypt(file_info &downfile_info, bool verbose, bool difout, std::string &diffile){
    std::ifstream encrypted(downfile_info.name+"_encrypted_download", std::ios::binary);
    if (!encrypted){
        std::cout<<"Error opening "<<downfile_info.name<<"_encrypted_download"<<'\n';
        exit(0);
    }

    std::string decrypted_path;

    if (difout){
        decrypted_path=diffile;
    }
    else{
        decrypted_path=downfile_info.name;
    }

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

    std::cout<<"decrypted: "<<downfile_info.name+"_encrypted_download"<<" to: "<<decrypted_path<<'\n';
}
