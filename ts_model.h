#ifndef __ts_model_h__
#define __ts_model_h__

//Qt
#include <QString>
#include <QXmlStreamAttributes>

//std
#include <iostream>
#include <vector>
#include <map>
#include <memory>

//algs
#include "efl_hash.h"

QT_BEGIN_NAMESPACE
    class QXmlStreamWriter;
    class QFile;
QT_END_NAMESPACE

//...............................................................................................................
// Visitors
//...............................................................................................................

struct document_node;
struct DTD_node;
struct element_node;
struct TS_node;

namespace visitors
{
    struct document_dump
    {
        document_dump(QXmlStreamWriter &writer) : m_writer(writer) {}

        void visit(const document_node *node) const;
        void visit(const DTD_node *node) const;
        void visit(const element_node *node) const;
		void visit(const TS_node *node) const;
		
    private:
        QXmlStreamWriter &m_writer;
    };

    //.........................................................................................
    
    typedef std::map<hash_t, QString>	map_hashQString;
    typedef std::map<QString, QString>	map_QStringQString;

    //.........................................................................................

    struct string_extractor_replacer
    {
        string_extractor_replacer(map_hashQString &vqs, bool with_unfinished, bool with_vanished, bool unfinished_only)
            : m_vqs(vqs), source(nullptr), translation(nullptr)
            , m_state(st_WaitForMessage)
            , m_with_unfinished(with_unfinished), m_with_vanished(with_vanished), m_unfinished_only(unfinished_only)
        {}

        void visit(const document_node *node);
        void visit(const DTD_node *node);
        void visit(element_node *node);
    
    private:
        enum EStates { st_WaitForMessage = 0x00, st_WaitForSource = 0x01, st_WaitForTranslation = 0x02, st_Complete = 0x04 };
    
    private:
        int m_state;
        element_node *source, *translation;

    private:
         map_hashQString &m_vqs;
         bool m_with_unfinished, m_with_vanished, m_unfinished_only;
    };

    //.........................................................................................

    struct back_string_replacer
    {
        back_string_replacer(const map_QStringQString &strings, const QString &langid) 
            : m_strings(strings)
			, m_langid(langid)
			, source(nullptr)
			, translation(nullptr)
			, m_state(st_WaitForMessage) 
        {}

        void visit(const document_node *node);
        void visit(const DTD_node *node);
        void visit(element_node *node);
		void visit(TS_node *node);
    
    private:
        enum EStates { st_WaitForMessage = 0x00, st_WaitForSource = 0x01, st_WaitForTranslation = 0x02, st_Complete = 0x04 };

    private:
        int m_state;
        element_node *source, *translation;

    private:
        map_QStringQString m_strings;
		const QString m_langid;
    };
}

//...............................................................................................................


struct base_node : std::enable_shared_from_this<base_node>
{
    friend visitors::document_dump;
    friend visitors::string_extractor_replacer;
    friend visitors::back_string_replacer;

    enum ENodeType {
            nt_Document         = 0x10000000
        ,   nt_DTD              = 0x01000000
        ,   nt_Element          = 0x00001000
    };

    typedef std::shared_ptr<base_node> base_node_ptr;
    typedef std::vector<base_node_ptr> nodes_t;

    base_node() : m_parent(nullptr) {}

    virtual ENodeType kind() const = 0;
    
    virtual void visit(const visitors::document_dump &visitor) const = 0;
	virtual void visit(visitors::string_extractor_replacer &/*visitor*/) {}
    virtual void visit(visitors::back_string_replacer &visitor) = 0;

    base_node_ptr     add_child(base_node_ptr ptr)
    {
        m_childs.push_back(ptr);
        ptr->m_parent = shared_from_this();
        return ptr;
    }
    base_node_ptr parent() const { return m_parent; }

private:
    nodes_t m_childs;
    base_node_ptr m_parent;
};

//...............................................................................................................

struct document_node : base_node
{
    document_node() : base_node() {}
    virtual ENodeType kind() const { return nt_Document; }
    virtual void visit(const visitors::document_dump &visitor) const { visitor.visit(this); }
    virtual void visit(visitors::string_extractor_replacer &visitor) { visitor.visit(this); }
    virtual void visit(visitors::back_string_replacer &visitor) { visitor.visit(this); }
};

//...............................................................................................................

struct DTD_node : base_node
{
    DTD_node(QString systemId) : base_node(), m_systemId(systemId) {}
    virtual ENodeType kind() const { return nt_DTD; }
    virtual void visit(const visitors::document_dump &visitor) const { visitor.visit(this); }
    virtual void visit(visitors::string_extractor_replacer &visitor) { visitor.visit(this); }
    virtual void visit(visitors::back_string_replacer &visitor) { visitor.visit(this); }

    const QString & id() const { return m_systemId; }
private:
    QString m_systemId;
};

//...............................................................................................................

struct element_node : base_node
{
    enum EElementNodeType { ent_element, ent_message, ent_source, ent_translation };

    element_node(EElementNodeType ent, const QString &name, const QXmlStreamAttributes &attrs) 
        : m_element_node_type(ent), m_name(name), m_attributes(attrs) 
    {}
    virtual ENodeType kind() const { return nt_Element; }
    virtual void visit(const visitors::document_dump &visitor) const { visitor.visit(this); }
    virtual void visit(visitors::string_extractor_replacer &visitor) { visitor.visit(this); }
    virtual void visit(visitors::back_string_replacer &visitor) { visitor.visit(this); }

    EElementNodeType element_node_type() const { return m_element_node_type; }
    
    void set_text(const QString &text) { m_text = text; }
    const QString & text() const { return m_text; }

    const QString & name() const { return m_name; }
    const QXmlStreamAttributes & attributes() const { return m_attributes; }
    
protected:
    QString m_name;
    QXmlStreamAttributes m_attributes;
    QString m_text;
    EElementNodeType m_element_node_type;
};

//...............................................................................................................

struct TS_node : element_node
{
	TS_node(const QString &name, const QXmlStreamAttributes &attrs)
		: element_node(element_node::ent_element, name, attrs)
	{}

	virtual void visit(visitors::back_string_replacer &visitor) { visitor.visit(this); }

	void replace_attribute_value(const QString &att_name, const QString &value)
	{
		QXmlStreamAttributes::iterator it = std::find_if(m_attributes.begin(), m_attributes.end(), [&att_name](const QXmlStreamAttribute &att){ return att_name == att.name(); });

		if(it != m_attributes.end()) {
			*it = QXmlStreamAttribute(it->namespaceUri().toString(), it->name().toString(), value);			
		}
	}
};


#endif // TS_MODEL

