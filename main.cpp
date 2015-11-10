#include <iostream>
#include <assert.h>
#include <algorithm>

//model
#include "ts_model.h"

//Qt
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>


void toTXT(const QString &inputFile, const QString &outputDir, QCommandLineParser &cmlp);
void toXML(const QString &inputDir, const QString &outputFile, QCommandLineParser &cmlp);


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("td_tool");
    QCoreApplication::setApplicationVersion("1.0");

    enum { input = 0, output, mode };

    QCommandLineParser cmlp;
    cmlp.setApplicationDescription("ts_tool.exe: CODIJY 2015");
    cmlp.addHelpOption();
    cmlp.addOption(QCommandLineOption("src", "Source file or directory"));
    cmlp.addOption(QCommandLineOption("dst", "Destination file or directory"));
    cmlp.addOption(QCommandLineOption("mode", "Mode: TXT i.e. ts to txt file, XML txt to ts"));

    cmlp.process(app);

    QStringList args = cmlp.positionalArguments();

    if(3 != args.size()) {
        std::cerr << "You may use at least 3 args" << std::endl;
        cmlp.showHelp(-1);
    }

    if("TXT" == args[mode])
    {
        toTXT(args[input], args[output], cmlp);
    }
    else if("XML" == args[mode])
    {
        toXML(args[input], args[output], cmlp);
    }
    else
    {
        std::cerr << "Invalide mode" << std::endl;
        cmlp.showHelp(-1);
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
	
	const QString rgxp("^(?<id>\\\[[A-F0-9]{8}\\\])\\s*\\\"(?<text>.*)\\\"$");
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

void toTXT(const QString &inputFile, const QString &outputDir, QCommandLineParser &cmlp)
{
	using namespace visitors;

    QFileInfo fiI(inputFile);
    if(!fiI.exists()) {
        std::cerr << "Input file not exist!" << std::endl;
        cmlp.showHelp(-1);
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
        cmlp.showHelp(-1);
    }
    
    QFile oFile(outputXmlFileName);
    oFile.open(QIODevice::WriteOnly);
	
	//pares ts file
	base_node::base_node_ptr root = parse_ts_file(inputFile);

	//replace strings
	map_hashQString strings;
	string_extractor_replacer ser(strings, false);
	root->visit(ser);
	
	//write text file
	QFile sFile(outputTextFile);
	sFile.open(QIODevice::WriteOnly|QIODevice::Text);
	QTextStream txts(&sFile);
		
	std::for_each(strings.begin(), strings.end(), [&txts](const map_hashQString::value_type &vt){ txts << vt.second << "\n"; });

	//write modified ts file
    QXmlStreamWriter xmlWriter(&oFile);
    xmlWriter.setAutoFormatting(true);

	document_dump ddv(xmlWriter);
	root->visit(ddv);
}

void toXML(const QString &inputDir, const QString &outputFile, QCommandLineParser &cmlp)
{
	using namespace visitors;

	QFileInfo fiI(inputDir);
	if(!fiI.exists()) {
		std::cerr << "Input directory not exist!" << std::endl;
		cmlp.showHelp(-1);
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
		cmlp.showHelp(-1);
	}

	//pares ts file
	base_node::base_node_ptr root = parse_ts_file(tsFile);

	//parse txt file
	map_QStringQString strings;
	if(!parse_txt_file(txtFile, strings)) {
		std::cerr << "Parsing error: " << tsFile.toStdString() << " !" << std::endl;
		cmlp.showHelp(-1);
	}

	//replace strings
	back_string_replacer bsr(strings);
	root->visit(bsr);

	//dump to file
	QFile oFile(outputFile);
	oFile.open(QIODevice::WriteOnly);

	QXmlStreamWriter xmlWriter(&oFile);
	xmlWriter.setAutoFormatting(true);

	document_dump ddv(xmlWriter);
	root->visit(ddv);
}
