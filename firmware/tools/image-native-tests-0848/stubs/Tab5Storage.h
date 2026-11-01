#pragma once
extern bool storageAvailable;
class Tab5StorageUse{public:explicit Tab5StorageUse(bool=false){}explicit operator bool()const{return storageAvailable;}};
