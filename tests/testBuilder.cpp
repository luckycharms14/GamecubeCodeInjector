#include "catch.hpp"

#include "../src/HelperFunctions/HelperFunctions.h"
#include "../src/MainProgram/Parser.h"

#include <iostream>

TEST_CASE("testing functions in Builder.cpp")
{
    std::cout << "\033[33m" <<
    "\n========= Testing Builder.cpp =========\n"
    << "\033[39m" << std::endl;

    /* arguments for testing */
    Arguments args;

    args.cmdOptions.insert(std::make_pair("--ar", "../tests/lib"));
    args.cmdOptions.insert(std::make_pair("--output",
        "../tests/libtest.a"));

    args.cmdOptions.insert(std::make_pair("--inject", "../tests/source"));
    args.libs.push_back("../tests/lib1.a");
    args.includePaths.push_back("../tests/include");

    args.cmdOptions.insert(std::make_pair("--config-file",
        "../tests/config.ini"));
    args.cmdOptions.insert(std::make_pair("--game-id", "2"));
    ConfigParser::parse(args);

    /** create test library **/

    /* remove old library */
    System::runCMD(System::rm + " ../tests/libtest.a");
    
    /* create library */
    REQUIRE_NOTHROW(Builder::buildLibrary(args));

    /* get sections */
    auto libSections = ObjectFile::getNamedSections("../tests/libtest.a");

    /* check section names */
    REQUIRE(libSections.size() == 8);
    REQUIRE(libSections[0] == "text0");
    REQUIRE(libSections[1] == "attr0");
    REQUIRE(libSections[2] == "text1");
    REQUIRE(libSections[3] == "rodata1");
    REQUIRE(libSections[4] == "attr1");
    REQUIRE(libSections[5] == "text2");
    REQUIRE(libSections[6] == "rodata2");
    REQUIRE(libSections[7] == "attr2");

    /** test adding original instruction **/

    /* create sample code */
    ASMcode testCode;
    testCode.push_back(std::make_pair(0x50, 0xabc));        
    testCode.push_back(std::make_pair(0x75, 0xabc));
    testCode.push_back(std::make_pair(0x100, 0xabc));
    testCode.push_back(std::make_pair(0x803fa3ec, 0x60000000));
    testCode.push_back(std::make_pair(0x150, 0xabc));
    testCode.push_back(std::make_pair(0x200, 0xabc));

    /* overwrite nop */
    REQUIRE_NOTHROW(Builder::addOriginalInstruction(testCode, args));
    REQUIRE(testCode[3].first == 0x803fa3ec);
    REQUIRE(testCode[3].second == 0x7ee3bb78);

    /* should return error - instruction not nop */
    REQUIRE_THROWS_AS(Builder::addOriginalInstruction(testCode, args),
        std::runtime_error);

    /* compile source files, return object files */
    auto objects = Builder::getObjectFiles(args.cmdOptions["--inject"], 
        args.includePaths, args.libs);

    REQUIRE(objects.size() == 3);    
    REQUIRE(objects[0] == "../tests/source/source1.o");    
    REQUIRE(objects[1] == "../tests/source/source2.o");
    REQUIRE(objects[2] == "../tests/lib1.a");

    /* find addresses for each section */
    auto sections = Builder::getSectionAddresses(objects, args);

    REQUIRE(sections.size() == 13);

    REQUIRE(sections[0].path == "../tests/source/source1.o (text)");
    REQUIRE(sections[1].path == "../tests/source/source1.o (rodata)");
    REQUIRE(sections[2].path == "../tests/source/source1.o (attr)");    
    REQUIRE(sections[3].path == "../tests/source/source2.o (text)");
    REQUIRE(sections[4].path == "../tests/source/source2.o (attr)");
    REQUIRE(sections[5].path == "../tests/lib1.a (text0)");
    REQUIRE(sections[6].path == "../tests/lib1.a (attr0)");
    REQUIRE(sections[7].path == "../tests/lib1.a (text1)");
    REQUIRE(sections[8].path == "../tests/lib1.a (rodata1)");
    REQUIRE(sections[9].path == "../tests/lib1.a (attr1)");
    REQUIRE(sections[10].path == "../tests/lib1.a (text2)");
    REQUIRE(sections[11].path == "../tests/lib1.a (rodata2)");
    REQUIRE(sections[12].path == "../tests/lib1.a (attr2)");

    REQUIRE(sections[0].size == 0xc0);
    REQUIRE(sections[1].size == 0x14);
    REQUIRE(sections[2].size == 0x10);    
    REQUIRE(sections[3].size == 0x2c);
    REQUIRE(sections[4].size == 0x10);        
    REQUIRE(sections[5].size == 0x0);
    REQUIRE(sections[6].size == 0x0);
    REQUIRE(sections[7].size == 0x258);
    REQUIRE(sections[8].size == 0x3c);
    REQUIRE(sections[9].size == 0x10);
    REQUIRE(sections[10].size == 0x0);
    REQUIRE(sections[11].size == 0x0);
    REQUIRE(sections[12].size == 0x0);

    REQUIRE(sections[0].address == 0x803fa650);
    REQUIRE(sections[1].address == 0x8030024c);    
    REQUIRE(sections[2].address == 0x80300278);
    REQUIRE(sections[3].address == 0x8030021c);
    REQUIRE(sections[4].address == 0x803fa714);            
    REQUIRE(sections[5].address == 0x0);
    REQUIRE(sections[6].address == 0x0);
    REQUIRE(sections[7].address == 0x803fa3f4);        
    REQUIRE(sections[8].address == 0x803001dc); 
    REQUIRE(sections[9].address == 0x80300264);       
    REQUIRE(sections[10].address == 0x0);
    REQUIRE(sections[11].address == 0x0);
    REQUIRE(sections[12].address == 0x0);

    /* add stack setup files */
    Builder::addStackSetup(sections, args);

    REQUIRE(sections.size() == 15);

    REQUIRE(sections[13].path == "stack_setup.o");
    REQUIRE(sections[13].address == 0x803fa3e8);

    REQUIRE(sections[14].path == "inject_point.o");
    REQUIRE(sections[14].address == 0x80377998);
    
    /* link all code in sections */
    System::runCMD(System::rm + " final.out");
    ASMcode code = Builder::getLinkedCode(sections);

    REQUIRE(code.size() == 244);

    REQUIRE(code[0].first == 0x803fa650);
    REQUIRE(code[0].second == 0x9421ffe8);

    REQUIRE(code[68].first == 0x80300244);
    REQUIRE(code[68].second == 0x4e800020);

    REQUIRE(code[69].first == 0x803fa714);
    REQUIRE(code[69].second == 0x41000000);

    REQUIRE(code[140].first == 0x803fa500);
    REQUIRE(code[140].second == 0x3920ffff);

    REQUIRE(code[180].first == 0x803fa5a0);
    REQUIRE(code[180].second == 0xc8080208);

    REQUIRE(code[243].first == 0x80377998);
    REQUIRE(code[243].second == 0x48082a51);
}