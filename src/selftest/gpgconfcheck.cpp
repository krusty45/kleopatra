/* -*- mode: c++; c-basic-offset:4 -*-
    selftest/gpgconfcheck.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2008 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "gpgconfcheck.h"

#include "implementation_p.h"

#include <utils/gnupg-helper.h>
#include <utils/hex.h>

#include "kleopatra_debug.h"
#include <KLocalizedString>

#include <QProcess>
#include <QDir>

#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>


using namespace Kleo;
using namespace Kleo::_detail;

namespace
{

class GpgConfCheck : public SelfTestImplementation
{
    QString m_component;
public:
    explicit GpgConfCheck(const char *component)
        : SelfTestImplementation(i18nc("@title", "%1 Configuration Check", component  && * component ? QLatin1String(component) : QLatin1String("gpgconf"))),
          m_component(QLatin1String(component))
    {
        runTest();
    }

    void runTest()
    {
        const auto conf = QGpgME::cryptoConfig();
        QString message;
        m_passed = true;

        if (!conf) {
            message = QStringLiteral ("Could not be started.");
            m_passed = false;
        } else if (m_component.isEmpty() && conf->componentList().empty()) {
            message = QStringLiteral ("Could not list components.");
            m_passed = false;
        } else if (!m_component.isEmpty()) {
            const auto comp = conf->component (m_component);
            if (!comp) {
                message = QStringLiteral ("Binary could not be found.");
                m_passed = false;
            } else if (comp->groupList().empty()) {
                // If we don't have any group it means that list-options
                // for this component failed.
                message = QStringLiteral ("The configuration file is invalid.");
                m_passed = false;
            }
        }

        if (!m_passed) {
            m_error = i18nc("self-test did not pass", "Failed");
            m_explaination =
                i18n("There was an error executing the GnuPG configuration self-check for %2:\n"
                     "  %1\n"
                     "You might want to execute \"gpgconf %3\" on the command line.\n",
                     message, m_component.isEmpty() ? QStringLiteral("GnuPG") : m_component,
                     QStringLiteral("--check-options ") + (m_component.isEmpty() ? QStringLiteral("") : m_component));

            // To avoid modifying the l10n
            m_explaination.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
        }
    }

};
}

std::shared_ptr<SelfTest> Kleo::makeGpgConfCheckConfigurationSelfTest(const char *component)
{
    return std::shared_ptr<SelfTest>(new GpgConfCheck(component));
}
