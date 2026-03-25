#include <iostream>
#include <cstdio>

int main() {
    FILE* pipe=popen("find . -type f | grep \"discord_upload_\"", "r");
    if (!pipe){
        std::cout<<"Failed to run find . -type f | grep \"discord_upload_\"\n";
        return 0;
    }

    char buffer[256]; //sets buffer for one line
    while (fgets(buffer, sizeof(buffer), pipe)!=nullptr){ //fgets when reaches buffer end stop pointing to place in buffer so becomes nullptr
        std::string filepath(buffer);

        //Remove the newline in buffer
        if (!filepath.empty() && filepath[filepath.length()-1]=='\n'){
            filepath.erase(filepath.length()-1, 1);
        }

        std::string rmfile="rm "+filepath;
        system(rmfile.c_str());
    }

    pclose(pipe);
    
    return 0;
}