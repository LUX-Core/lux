/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "adminauthorization_p.h"

#include <QtCore/QSettings>
#include <QtCore/QVariant>
#include <QtCore/QDir>
#include <QtCore/qt_windows.h>

using namespace QtLuxUpdater;

static QString qt_create_commandline(const QStringList &arguments);

struct DeCoInitializer
{
	DeCoInitializer()
		: neededCoInit(CoInitialize(NULL) == S_OK)
	{}

	~DeCoInitializer()
	{
		if (neededCoInit)
			CoUninitialize();
	}
	bool neededCoInit;
};

bool AdminAuthorization::hasAdminRights()
{
	SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
	PSID adminGroup;
	// Initialize SID.
	if (!AllocateAndInitializeSid(&authority,
								  2,
								  SECURITY_BUILTIN_DOMAIN_RID,
								  DOMAIN_ALIAS_RID_ADMINS,
								  0, 0, 0, 0, 0, 0,
								  &adminGroup))
		return false;

	BOOL isInAdminGroup = FALSE;
	if (!CheckTokenMembership(0, adminGroup, &isInAdminGroup))
		isInAdminGroup = FALSE;

	FreeSid(adminGroup);
	return (bool)isInAdminGroup;
}

bool AdminAuthorization::executeAsAdmin(const QString &program, const QStringList &arguments)
{
	DeCoInitializer _;

	// AdminAuthorization::execute uses UAC to ask for admin privileges. If the user is no
	// administrator yet and the computer's policies are set to not use UAC (which is the case
	// in some corporate networks), the call to execute() will simply succeed and not at all
	// launch the child process. To avoid this, we detect this situation here and return early.
	if (!hasAdminRights()) {
		QLatin1String key("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System");
		QSettings registry(key, QSettings::NativeFormat);
		const QVariant enableLUA = registry.value(QLatin1String("EnableLUA"));
		if ((enableLUA.type() == QVariant::Int) && (enableLUA.toInt() == 0))
			return false;
	}

	const QString file = QDir::toNativeSeparators(program);
	const QString args = qt_create_commandline(arguments);

	SHELLEXECUTEINFOW shellExecuteInfo;
	ZeroMemory(&shellExecuteInfo, sizeof(SHELLEXECUTEINFOW));
	shellExecuteInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
	shellExecuteInfo.lpVerb = L"runas";
	shellExecuteInfo.lpFile = (wchar_t *)file.utf16();
	shellExecuteInfo.lpParameters = (wchar_t *)args.utf16();
	shellExecuteInfo.nShow = SW_SHOW;

	if (ShellExecuteExW(&shellExecuteInfo))
		return true;
	else
		return false;
}

QString qt_create_commandline(const QStringList &arguments)
{
	QString args;
	for (int i = 0; i < arguments.size(); ++i) {
		QString tmp = arguments.at(i);
		// in the case of \" already being in the string the \ must also be escaped
		tmp.replace(QLatin1String("\\\""), QLatin1String("\\\\\""));
		// escape a single " because the arguments will be parsed
		tmp.replace(QLatin1Char('\"'), QLatin1String("\\\""));
		if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
			// The argument must not end with a \ since this would be interpreted
			// as escaping the quote -- rather put the \ behind the quote: e.g.
			// rather use "foo"\ than "foo\"
			QString endQuote(QLatin1Char('\"'));
			int i = tmp.length();
			while (i > 0 && tmp.at(i - 1) == QLatin1Char('\\')) {
				--i;
				endQuote += QLatin1Char('\\');
			}
			args += QLatin1String(" \"") + tmp.left(i) + endQuote;
		} else {
			args += QLatin1Char(' ') + tmp;
		}
	}
	return args;
}
