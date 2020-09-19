import argparse
import subprocess
from datetime import date
import os
import platform

parser = argparse.ArgumentParser()
parser.add_argument('--name', type = str)
parser.add_argument('--version', type = str)
parser.add_argument('--site_url', type = str)
parser.add_argument('--logo', type = str)
parser.add_argument('--watermark', type = str)
parser.add_argument('--repository', type = str)
parser.add_argument('--installer_dir', type = str)

args = parser.parse_args()


f = open("./configure.ac", "r")
configure_lines = f.readlines()[:20]
f.close()

majon_version_line = None
minor_version_line = None
revision_version_line = None
build_version_line = None
ac_init_line = None

try:
    majon_version_line = next(filter(lambda x: "_CLIENT_VERSION_MAJOR" in x, configure_lines)).replace(" ", "").replace("\n", "").replace(")","").split(",")
    minor_version_line = next(filter(lambda x: "_CLIENT_VERSION_MINOR" in x, configure_lines)).replace(" ", "").replace("\n", "").replace(")","").split(",")
    revision_version_line = next(filter(lambda x: "_CLIENT_VERSION_REVISION" in x, configure_lines)).replace(" ", "").replace("\n", "").replace(")","").split(",")
    build_version_line = next(filter(lambda x: "_CLIENT_VERSION_BUILD" in x, configure_lines)).replace(" ", "").replace("\n", "").replace(")","").split(",")
    ac_init_line = next(filter(lambda x: "AC_INIT" in x, configure_lines)).replace(" ", "").replace("\n", "").split(",")

except Exception as e:
    print(e)

commit_id = subprocess.check_output(['git', 'log', '--pretty=format:\'%h\'', '-n', '1']).strip().decode('ascii').replace("'","")
name = "LUX Wallet"
version = "v%s.%s.%s.%s-%s" % (majon_version_line[1], minor_version_line[1], revision_version_line[1], build_version_line[1], commit_id)
site_url = ac_init_line[4].replace("[","").replace("]","").replace(")","")
publisher = ac_init_line[0].replace("AC_INIT([","").replace("]","")
logo = "logo"
watermark = "watermark.png"

installer_dir = 'C:\\Qt\\QtIFW-3.2.2\\'
repository = ""

if args.repository is not None:
    repository = args['repository']

if args.installer_dir is not None:
    installer_dir = args['installer_dir']

config_xml_str = """<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>%s</Name>
    <Version>%s-%s</Version>
    <Title>%s</Title>
    <Publisher>%s</Publisher>
    <ProductUrl>%s</ProductUrl>
    <RunProgram>@TargetDir@/lux-qt.exe</RunProgram>
    <Logo>%s</Logo>
    <Watermark>%s</Watermark>
    <WizardStyle>Modern</WizardStyle>
    <StartMenuDir>%s</StartMenuDir>
    <TargetDir>@HomeDir@/%s</TargetDir>
    <InstallerWindowIcon>%s</InstallerWindowIcon>
    <InstallerApplicationIcon>%s</InstallerApplicationIcon>
    <RemoteRepositories>
        <Repository>
            <Url>%s</Url>
        </Repository>
    </RemoteRepositories>
</Installer>""" % (name, version, publisher, name, publisher, site_url, logo, watermark, publisher, publisher, logo, logo, repository)

package_xml_str = """<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>%s</DisplayName>
    <Version>%s-%s</Version>
    <ReleaseDate>%s</ReleaseDate>
    <Licenses>
        <License file="license.html" />
    </Licenses>
    <Default>script</Default>
    <Script>installscript.qs</Script>
</Package>""" % (name, version, publisher, date.today().strftime("%Y-%m-%d"))


text_file = open("./packager/config/config.xml", "w")
n = text_file.write(config_xml_str)
text_file.close()


text_file = open("./packager/packages/lux/meta/package.xml", "w")
n = text_file.write(package_xml_str)
text_file.close()


commit_id = subprocess.check_output(['git', 'log', '--pretty=format:\'%h\'', '-n', '1']).strip().decode('ascii').replace("'","")
if platform.system() == 'Windows':
    installer_cmd = [installer_dir + 'bin\\binarycreator.exe', '-c', 'packager\\config\\config.xml', '-p', 'packager\\packages', '-t', installer_dir +  'bin\\installerbase.exe', 'LuxInstaler']
else:
    installer_cmd = [installer_dir + 'bin/binarycreator', '-c', './packager/config/config.xml', '-p', './packager/packages', '-t', installer_dir +  'bin/installerbase', 'LuxInstaler']

process = subprocess.Popen(installer_cmd, stdout=subprocess.PIPE)
process.wait()