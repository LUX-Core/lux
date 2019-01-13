#ifndef QTLUXUPDATER_ADMINAUTHORIZATION_P_H
#define QTLUXUPDATER_ADMINAUTHORIZATION_P_H

#include "luxupdater/adminauthoriser.h"

namespace QtLuxUpdater
{

class AdminAuthorization : public AdminAuthoriser
{
public:
	bool hasAdminRights() override;
	bool executeAsAdmin(const QString &program, const QStringList &arguments) override;
};

}

#endif // QTLUXUPDATER_ADMINAUTHORIZATION_P_H
