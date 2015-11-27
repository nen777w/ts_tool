#include <iostream>
#include <assert.h>
#include <algorithm>

//model
#include "ts_model.h"

//Qt
#include <QString>
#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>

#define VERSION "2.3"

void toTXT(const QString &inputFile, const QString &outputDir, bool with_unfinished, bool with_vanished);
void toTS(const QString &inputDir, const QString &outputFile);

enum EArgID {
        arg_unknown = -1
    ,   arg_help = 0, arg_src, arg_dst, arg_mode, arg_with_unfinished, arg_with_vanished
};

struct argument_info
{
    EArgID id;
    const char *name; 
    const char *description;
    bool flag;
};

//SAME ORDER AS in EArgID
static const argument_info args[] = {
        {arg_help, "--help", "Show this help", true}
    ,   {arg_src, "--src", "Source file or directory", false}
    ,   {arg_dst, "--dst", "Destination file or directory", false}
    ,   {arg_mode, "--mode", "Mode: TXT i.e. .ts to .txt | TS i.e. .txt to .ts", false}
    ,   {arg_with_unfinished, "--with-unfinished", "Include unfinished translations. By default: ignore. [Work only in TXT mode]", true}
    ,   {arg_with_vanished, "--with-vanished", "Include obsolete translations. By default: ignore. [Work only in TXT mode]", true}
};

void show_help(int exit_code)
{
    std::cout << "ts_tool v" VERSION " CODIJY 2015" << std::endl;
    std::cout << "Arguments:" << std::endl;

    std::for_each(args, args+sizeof(args)/sizeof(argument_info), [](const argument_info &nfo)
        {
            std::cout << "\t" << nfo.name << " " << nfo.description << std::endl;
        }
    );

    exit(exit_code);
}

EArgID get_arg_id(const char *value)
{
    EArgID id = arg_unknown;
    std::any_of(args, args+sizeof(args)/sizeof(argument_info), [&id, value](const argument_info &nfo) -> bool
        {
            if(0 == strcmp(nfo.name, value)) {
                id = nfo.id;
            }

            return arg_unknown != id;
        }
    );

    return id;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("td_tool");
    QCoreApplication::setApplicationVersion(VERSION);

    QString src, dst, mode;
    bool with_unfinished = false, with_vanished = false;

    if(1 == argc) {
        show_help(0);
    }

    QString *value = nullptr;
    for(int n = 1; n < argc; ++n)
    {
        EArgID argId = get_arg_id(argv[n]);
        switch(argId)
        {
        case arg_unknown: show_help(-1); break;
        case arg_src: value = &src; break;
        case arg_dst: value = &dst; break;
        case arg_mode: value = &mode; break;
        case arg_with_unfinished: with_unfinished = true; break;
        case arg_with_vanished: with_vanished = true; break;
        }

        if(value) {
            *value = argv[++n];
        }

        value = nullptr;
    }


    if(src.isEmpty() || dst.isEmpty() || mode.isEmpty()) {
        std::cerr << "You may use at least first 3 args" << std::endl;
        show_help(-1);
    }

    if("TXT" == mode)
    {
        toTXT(src, dst, with_unfinished, with_vanished);
    }
    else if("TS" == mode)
    {
        toTS(src, dst);
    }
    else
    {
        std::cerr << "Invalid mode" << std::endl;
        show_help(-1);
    }

    return 0;
}

base_node::base_node_ptr parse_ts_file(const QString &inputFile)
{
    QFile iFile(inputFile);
    iFile.open(QIODevice::ReadOnly);

    QXmlStreamReader xmlReader(&iFile);

    base_node::base_node_ptr root, current;
    QString text;

    enum EStates {
        st_Unstate = 0
        ,	st_WaitForStartElement = 0x01
        ,   st_WaitForText = 0x02
        ,   st_WaitForEndElement = 0x04
    };

    int states = st_WaitForStartElement;

    while(!xmlReader.atEnd())
    {
        QXmlStreamReader::TokenType tt = xmlReader.readNext();
        switch(tt)
        {
        case QXmlStreamReader::StartDocument:
            {
                root = std::make_shared<document_node>();
                current = root;
            } break;
        case QXmlStreamReader::DTD:
            {
                current->add_child(std::make_shared<DTD_node>("<!DOCTYPE TS>"));
            } break;
        case QXmlStreamReader::StartElement:
            {
                assert(states & st_WaitForStartElement);

                QString name = xmlReader.name().toString();
                QXmlStreamAttributes attrs = xmlReader.attributes();

                element_node::EElementNodeType ent = element_node::ent_element;

                if("message" == name) {
                    ent = element_node::ent_message;
                } else if("source" == name) {
                    ent = element_node::ent_source;
                } else if("translation" == name) {
                    ent = element_node::ent_translation;
                }

                current = current->add_child(std::make_shared<element_node>(ent, name, attrs));

                states = st_WaitForText|st_WaitForStartElement|st_WaitForEndElement;
            } break;
        case QXmlStreamReader::Characters:
            {
                if(states & st_WaitForText) 
                {
                    text = xmlReader.text().toString();
                    states = st_WaitForEndElement|st_WaitForStartElement;
                }
            } break;
        case QXmlStreamReader::EndElement:
            {
                assert(states & st_WaitForEndElement);
                assert(current->kind() & base_node::nt_Element);
                ((element_node*)current.get())->set_text(text);
                text.clear();
                states = st_WaitForStartElement|st_WaitForEndElement;
                current = current->parent();
            } break;
        }
    }

    return root;
}

bool parse_txt_file(const QString &inputFile, visitors::map_QStringQString &strings)
{
    QFile iFile(inputFile);
    iFile.open(QFile::ReadOnly|QFile::Text);
    QTextStream txts(&iFile);
    txts.setCodec("UTF-8");
    
    const QString rgxp("^(?<id>\\[\\[\\[[A-F0-9]{8}\\]\\]\\])\\s*[\\\",“,”](?<text>.*)[\\\",“,”]$");
    QRegularExpression rxp(rgxp);

    unsigned int line_counter = 0;

    while(!txts.atEnd())
    {
        QString str = txts.readLine();
        QRegularExpressionMatch rm = rxp.match(str);

        QString id		= rm.captured("id");
        QString text	= rm.captured("text");

        if(id.isEmpty() || text.isEmpty())
        {
            std::cerr << "Error in line: " << line_counter << " file: " << inputFile.toStdString().c_str() << std::endl;
            return false;
        }

        strings.insert(visitors::map_QStringQString::value_type(id, text));
    }	

    return true;
}

void toTXT(const QString &inputFile, const QString &outputDir, bool with_unfinished, bool with_vanished)
{
    using namespace visitors;

    QFileInfo fiI(inputFile);
    if(!fiI.exists()) {
        std::cerr << "Input file not exist!" << std::endl;
        show_help(-1);
    }

    QFileInfo fiO(outputDir);
    if(!fiO.exists()) {
        QDir().mkdir(outputDir);
    }

    QString outputXmlFileName = QDir(outputDir).path() + "/" + fiI.fileName();
    QString outputTextFile = QDir(outputDir).path() + "/" + fiI.baseName() + ".txt";
    
    unsigned int files_in_out_dir = QDir(outputDir).entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries).count();

    if( !fiO.exists() 
        || 2 < files_in_out_dir
        || (2 == files_in_out_dir && !QFileInfo(outputXmlFileName).exists() && !QFileInfo(outputTextFile).exists()) )
    {
        std::cerr << "Cant create output directory OR directory is not empty!" << std::endl;
        show_help(-1);
    }
    
    QFile oFile(outputXmlFileName);
    oFile.open(QIODevice::WriteOnly);
    
    //pares ts file
    base_node::base_node_ptr root = parse_ts_file(inputFile);

    //replace strings
    map_hashQString strings;
    string_extractor_replacer ser(strings, with_unfinished, with_vanished);
    root->visit(ser);
    
    //write text file
    QFile sFile(outputTextFile);
    sFile.open(QIODevice::WriteOnly|QIODevice::Text);
    QTextStream txts(&sFile);
    txts.setCodec("UTF-8");
        
    std::for_each(strings.begin(), strings.end(), [&txts](const map_hashQString::value_type &vt){ txts << vt.second << "\n"; });

    //write modified ts file
    QXmlStreamWriter xmlWriter(&oFile);
    xmlWriter.setAutoFormatting(true);

    document_dump ddv(xmlWriter);
    root->visit(ddv);
}

void toTS(const QString &inputDir, const QString &outputFile)
{
    using namespace visitors;

    QFileInfo fiI(inputDir);
    if(!fiI.exists()) {
        std::cerr << "Input directory not exist!" << std::endl;
        show_help(-1);
    }

    const QFileInfoList &fil = QDir(inputDir).entryInfoList(QDir::NoDotAndDotDot|QDir::AllEntries);
    unsigned int files_in_input_dir = fil.count();

    QString tsFile, txtFile;

    if(2 == files_in_input_dir)
    {
        QFileInfo if0(QDir(inputDir).path() + "/" + fil[0].baseName() + ".ts");
        QFileInfo if1(QDir(inputDir).path() + "/" + fil[0].baseName() + ".txt");

        if(if0.isFile() && if1.isFile())
        {
            tsFile = if0.filePath();
            txtFile = if1.filePath();
        }
    }

    if(2 < files_in_input_dir || 0 == files_in_input_dir || tsFile.isEmpty() || txtFile.isEmpty())
    {
        std::cerr << "Input directory should contain only txt and ts file with same name!" << std::endl;
        show_help(-1);
    }

    //pares ts file
    base_node::base_node_ptr root = parse_ts_file(tsFile);

    //parse txt file
    map_QStringQString strings;
    if(!parse_txt_file(txtFile, strings)) {
        std::cerr << "Parsing error: " << tsFile.toStdString() << " !" << std::endl;
        show_help(-1);
    }

    //replace strings
    back_string_replacer bsr(strings);
    root->visit(bsr);

    //dump to file
    QFile oFile(outputFile);
    oFile.open(QIODevice::WriteOnly);

    QXmlStreamWriter xmlWriter(&oFile);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.setCodec("UTF-8");

    document_dump ddv(xmlWriter);
    root->visit(ddv);
}
