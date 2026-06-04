#include "RPCPicture.h"

#include "RPCPictureTest.h"

std::string RPCPicture::name() {
    return "picture";
}

std::string RPCPicture::handle() {
    return RPCPictureTest::sampleJson();
}
