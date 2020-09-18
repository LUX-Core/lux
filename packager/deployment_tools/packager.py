
name = "LUX Wallet"
version = "v5.3.5.0-22d01d88"
site_url = "https://luxcore.io"
publisher = "Luxcore"
logo = "logo"
watermark = "watermark.png"


repository = "http://www.your-repo-location/packages/"

xmlstr = """<?xml version="1.0" encoding="UTF-8"?>
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


text_file = open("config/config.xml", "w")
n = text_file.write(xmlstr)
text_file.close()