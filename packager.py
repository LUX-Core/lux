import argparse
import subprocess
from datetime import date
import os
from pathlib import Path
import shutil
import stat

parser = argparse.ArgumentParser()
parser.add_argument('--repository', type = str)
parser.add_argument('--installer_dir', type = str)
parser.add_argument('--platform', type = str)


args = parser.parse_args()

valid_os_list = ['windows', 'linux', 'macos']
platform = ""

print("Platform Selected:",args.platform)

if args.platform != None:
    if args.platform in valid_os_list:
        platform = args.platform

if not platform in valid_os_list:
    print("Platform not selected:",valid_os_list)
    quit()

try:
    if args.platform == 'windows':
        shutil.copyfile('src/luxd.exe', 'packager/packages/lux/data/luxd.exe')
        shutil.copyfile('src/lux-cli.exe', 'packager/packages/lux/data/lux-cli.exe')
        shutil.copyfile('src/lux-tx.exe', 'packager/packages/lux/data/lux-tx.exe')
        shutil.copyfile('src/qt/lux-qt.exe', 'packager/packages/lux/data/lux-qt.exe')
    if args.platform in ['macos', 'linux']:
        os.system('strip src/luxd')
        os.system('strip src/lux-cli')
        os.system('strip src/lux-tx')
        os.system('strip src/qt/lux-qt')

        shutil.copyfile('src/luxd', 'packager/packages/lux/data/luxd')
        st = os.stat('packager/packages/lux/data/luxd')
        os.chmod('packager/packages/lux/data/luxd', st.st_mode | stat.S_IEXEC)
        shutil.copyfile('src/lux-cli', 'packager/packages/lux/data/lux-cli')
        st = os.stat('packager/packages/lux/data/lux-cli')
        os.chmod('packager/packages/lux/data/lux-cli', st.st_mode | stat.S_IEXEC)
        shutil.copyfile('src/lux-tx', 'packager/packages/lux/data/lux-tx')
        st = os.stat('packager/packages/lux/data/lux-tx')
        os.chmod('packager/packages/lux/data/lux-tx', st.st_mode | stat.S_IEXEC)
        shutil.copyfile('src/qt/lux-qt', 'packager/packages/lux/data/lux-qt')
        st = os.stat('packager/packages/lux/data/lux-qt')
        os.chmod('packager/packages/lux/data/lux-qt', st.st_mode | stat.S_IEXEC)

except Exception as e:
    print(e)
    quit()


f = open('./configure.ac', 'r')
configure_lines = f.readlines()[:20]
f.close()

majon_version_line = None
minor_version_line = None
revision_version_line = None
build_version_line = None
ac_init_line = None

try:
    majon_version_line = next(filter(lambda x: '_CLIENT_VERSION_MAJOR' in x, configure_lines)).replace(' ', '').replace('\n', '').replace(')', '').split(',')
    minor_version_line = next(filter(lambda x: '_CLIENT_VERSION_MINOR' in x, configure_lines)).replace(' ', '').replace('\n', '').replace(')', '').split(',')
    revision_version_line = next(filter(lambda x: '_CLIENT_VERSION_REVISION' in x, configure_lines)).replace(' ', '').replace('\n', '').replace(')', '').split(',')
    build_version_line = next(filter(lambda x: '_CLIENT_VERSION_BUILD' in x, configure_lines)).replace(' ', '').replace('\n', '').replace(')', '').split(',')
    ac_init_line = next(filter(lambda x: 'AC_INIT' in x, configure_lines)).replace(' ', '').replace('\n', '').split(',')

except Exception as e:
    print(e)

commit_id = subprocess.check_output(['git', 'log', '--pretty=format:\'%h\'', '-n', '1']).strip().decode('ascii').replace('\'','')
name = 'LUX Wallet'
version = 'v%s.%s.%s.%s-%s' % (majon_version_line[1], minor_version_line[1], revision_version_line[1], build_version_line[1], commit_id)
site_url = ac_init_line[4].replace('[', '').replace(']', '').replace(')', '')
publisher = ac_init_line[0].replace('AC_INIT([', '').replace(']', '')
logo = 'logo'
watermark = 'watermark.png'

repository = 'http://155.138.209.167/repo/' + platform
if args.repository is not None:
    repository = args['repository']



config_xml_str = """<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>%s</Name>
    <Version>%s-%s</Version>
    <Title>%s</Title>
    <Publisher>%s</Publisher>
    <ProductUrl>%s</ProductUrl>
    <RunProgram>@TargetDir@/lux-qt%s</RunProgram>
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
</Installer>""" % (name, version, publisher, name, publisher, site_url, '.exe' if platform == 'windows' else '', logo, watermark, publisher, publisher, logo, logo, repository)

package_xml_str = """<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <Name>%s</Name>
    <DisplayName>%s</DisplayName>
    <Version>%s-%s</Version>
    <ReleaseDate>%s</ReleaseDate>
    <Licenses>
        <License file="license.html" />
    </Licenses>
    <Default>script</Default>
    <Script>installscript.qs</Script>
</Package>""" % (name, name, version, publisher, date.today().strftime("%Y-%m-%d"))


text_file = open('./packager/config/config.xml', 'w')
n = text_file.write(config_xml_str)
text_file.close()


text_file = open('./packager/packages/lux/meta/package.xml', 'w')
n = text_file.write(package_xml_str)
text_file.close()

installer_cmd = []
repo_cmd = []
commit_id = subprocess.check_output(['git', 'log', '--pretty=format:\'%h\'', '-n', '1']).strip().decode('ascii').replace('\'', '')
#if platform == 'windows':
#    installer_dir = 'C:\\Qt\\QtIFW-3.2.2\\'
#    if args.installer_dir is not None:
#        installer_dir = args.installer_dir
#    installer_cmd = [installer_dir + 'bin\\binarycreator.exe', '--online-only', '-c', 'packager\\config\\config.xml', '-p', 'packager\\packages', '-t', installer_dir +  'bin\\installerbase.exe', 'LuxInstaller']
#    repo_cmd = [installer_dir + 'bin\\repogen.exe', '-p', 'packager\\packages', 'repo' + platform]
#
#else:
#    installer_dir = str(Path.home()) + '/Qt/QtIFW-3.2.2/'
#    if args.installer_dir is not None:
#        installer_dir = args.installer_dir
#    installer_cmd = [installer_dir + 'bin/binarycreator', '--online-only', '-c', './packager/config/config.xml', '-p', './packager/packages', '-t', installer_dir +  'bin/installerbase', 'LuxInstaller']
#    repo_cmd = [installer_dir + 'bin/repogen', '-p', './packager/packages', 'repo' + platform]


installer_dir = str(Path.home()) + '/Qt/QtIFW-3.2.2/'
if args.installer_dir != None:
    installer_dir = args.installer_dir

repo_cmd = [installer_dir + 'bin/repogen', '-p', './packager/packages', '-i', 'lux', 'repo' + platform]
installer_cmd = [installer_dir + 'bin/binarycreator', '-c', './packager/config/config.xml', '-p', './packager/packages', '-t', installer_dir +  'bin/installerbase', 'LuxInstaller']



process = subprocess.Popen(installer_cmd, stdout=subprocess.PIPE)
process.wait()
try:
    shutil.rmtree('repo' + platform)
except Exception as e:
    print(e)
    pass

process = subprocess.Popen(repo_cmd, stdout=subprocess.PIPE)
process.wait()
