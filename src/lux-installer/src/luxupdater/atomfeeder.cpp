#include "atomfeeder.h"
#include "../updatecontroller.h"
#include "../dialogmaster.h"
#include "../updatecontroller_p.h"
using namespace QtLuxUpdater;

AtomFeeder::AtomFeeder(const QString &url, QObject *parent)
	:QObject(parent)
{
	this->m_url = url;
}

AtomFeeder::~AtomFeeder()
{}

void AtomFeeder::getXmlFeed()
{
	QUrl url(this->m_url);
	QNetworkRequest request;
	request.setUrl(url);
	m_currentReply = NULL;
	m_currentReply = m_networkManager.get(request);

	connect(&m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));
}

const QList< QMap<QString,QString> > &AtomFeeder::getVersionList()
{
	return entries;
}

int AtomFeeder::getVersionListCount()
{
	return entries.count();
}

QString AtomFeeder::getVersion(int position)
{
    if (position >= entries.count())
        return "";
    QString version = entries[position]["id"];
    int pos = version.lastIndexOf('/');
    if (pos >= 0)
        version = version.mid(pos+1);
    return version;
}

void AtomFeeder::onResult(QNetworkReply* reply)
{
	if(m_currentReply->error() != QNetworkReply::NoError)
	{
        running = false;
	    wasCanceled = true;
        isUpdaterRunning = false;
        gUpdatesProgress->hideProgressDialog(QMessageBox::Critical);
		DialogMaster::warningT(win,
								   tr("Check for Updates"),
								   tr("Cannot connect to server. Please check your network connection!"));
		return;
	}
	QString data = (QString) reply->readAll();

	QXmlStreamReader xml(data);

	/* We'll parse the XML until we reach end of it.*/
    while(!xml.atEnd() &&
            !xml.hasError()) {
        /* Read next element.*/
        QXmlStreamReader::TokenType token = xml.readNext();
        
        /* If token is just StartDocument, we'll go to next.*/
        if(token == QXmlStreamReader::StartDocument) {
            continue;
        }
        
        /* If token is StartElement, we'll see if we can read it.*/
        if(token == QXmlStreamReader::StartElement) {
            /* If it's named entry, we'll dig the information from there.*/
            if(xml.name() == "entry") {
                entries.append(this->parseEntry(xml));
            }
        }
    }
    
    /* Removes any device() or data from the reader
     * and resets its internal state to the initial state. */
    xml.clear();

    emit getVersionListDone();
}

QMap<QString, QString> AtomFeeder::parseEntry(QXmlStreamReader& xml)
{
    QMap<QString, QString> entry;
    /* Let's check that we're really getting a entry. */
    if(xml.tokenType() != QXmlStreamReader::StartElement &&
            xml.name() == "entry") {
        return entry;
    }
    
    /* Next element... */
    xml.readNext();
    /*
     * We're going to loop over the things because the order might change.
     * We'll continue the loop until we hit an EndElement named entry.
     */
    while(!(xml.tokenType() == QXmlStreamReader::EndElement &&
            xml.name() == "entry")) {
        if(xml.tokenType() == QXmlStreamReader::StartElement) {
            /* We've found id. */
            if(xml.name() == "id") {
                this->addElementDataToMap(xml, entry);
            }
            /* We've found title. */
            if(xml.name() == "title") {
                this->addElementDataToMap(xml, entry);
            }
            /* We've found updated date. */
            if(xml.name() == "updated") {
                this->addElementDataToMap(xml, entry);
            }
        }
        /* ...and next... */
        xml.readNext();
    }
    return entry;
}

void AtomFeeder::addElementDataToMap(QXmlStreamReader& xml,
                                      QMap<QString, QString>& map) const
{
    /* We need a start element, like <foo> */
    if(xml.tokenType() != QXmlStreamReader::StartElement) {
        return;
    }
    /* Let's read the name... */
    QString elementName = xml.name().toString();
    /* ...go to the next. */
    xml.readNext();
    /*
     * This elements needs to contain Characters so we know it's
     * actually data, if it's not we'll leave.
     */
    if(xml.tokenType() != QXmlStreamReader::Characters) {
        return;
    }
    /* Now we can add it to the map.*/
    map.insert(elementName, xml.text().toString());
}

void AtomFeeder::start()
{
    getXmlFeed();
}

void AtomFeeder::stop()
{
    // TODO: stop feeder request
}