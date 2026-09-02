#!/usr/bin/env python3

# Copyright 2017-2018 Jussi Pakkanen et al
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import annotations

import json
import os
import platform
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from glob import glob
from pathlib import Path
from typing import Any

sys.path.append(os.getcwd())


def gen_guid() -> str:
    return str(uuid.uuid4()).upper()


@dataclass(slots=True)
class Node:
    dirs: list[str]
    files: list[str]

    def __post_init__(self) -> None:
        assert isinstance(self.dirs, list)
        assert isinstance(self.files, list)


class PackageGenerator:

    def __init__(self, jsonfile: str) -> None:
        with open(jsonfile, 'rb') as file:
            jsondata: dict[str, Any] = json.load(file)
        self.product_name = jsondata['product_name']
        self.manufacturer = jsondata['manufacturer']
        self.version = jsondata['version']
        self.comments = jsondata['comments']
        self.installdir = jsondata['installdir']
        self.license_file = jsondata['license_file']
        self.name = jsondata['name']
        self.guid = '*'
        self.upgrade_guid = jsondata['upgrade_guid']
        self.basename = jsondata['name_base']
        self.need_msvcrt = jsondata.get('need_msvcrt', False)
        self.addremove_icon = jsondata.get('addremove_icon', None)
        self.startmenu_shortcut = jsondata.get('startmenu_shortcut', None)
        self.desktop_shortcut = jsondata.get('desktop_shortcut', None)
        self.main_xml = self.basename + '.wxs'
        self.main_o = self.basename + '.wixobj'
        if 'arch' in jsondata:
            self.arch = jsondata['arch']
        else:
            # rely on the environment variable since python architecture may not be the same as system architecture
            if 'PROGRAMFILES(X86)' in os.environ:
                self.arch = 64
            else:
                self.arch = 32 if '32' in platform.architecture()[0] else 64
        self.final_output = '%s-%s-%d.msi' % (self.basename, self.version, self.arch)
        if self.arch == 64:
            self.progfile_dir = 'ProgramFiles64Folder'
            if platform.system() == "Windows":
                redist_glob = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Redist\\MSVC\\*\\MergeModules\\Microsoft_VC141_CRT_x64.msm'
            else:
                redist_glob = '/usr/share/msicreator/Microsoft_VC141_CRT_x64.msm'
        else:
            self.progfile_dir = 'ProgramFilesFolder'
            if platform.system() == "Windows":
                redist_glob = 'C:\\Program Files\\Microsoft Visual Studio\\2017\\Community\\VC\\Redist\\MSVC\\*\\MergeModules\\Microsoft_VC141_CRT_x86.msm'
            else:
                redist_glob = '/usr/share/msicreator/Microsoft_VC141_CRT_x86.msm'
        trials = glob(redist_glob)
        if self.need_msvcrt:
            if len(trials) != 1:
                sys.exit('There are more than one potential redist dirs.')
            self.redist_path = trials[0]
        self.component_num = 0
        self.registry_entries = jsondata.get('registry_entries', None)
        self.install_location_registry = jsondata.get('install_location_registry', None)
        self.major_upgrade = jsondata.get('major_upgrade', None)
        self.parts = jsondata['parts']
        self.feature_components = {}
        self.feature_properties = {}
        self.registry_action_keys: set[tuple[str, str]] = set()
        self.args1: list[str] = []
        self.args2: list[str] = []
        if self.major_upgrade is not None and self.major_upgrade.get('AllowSameVersionUpgrades') == 'yes':
            # WiX ICE61 cannot distinguish the intentional same-version major-upgrade policy from a
            # version-range authoring mistake. Keep all other linker warnings enabled.
            self.args2.append('-sice:ICE61')

    def generate_files(self) -> None:
        self.root = ET.Element('Wix', {'xmlns': 'http://schemas.microsoft.com/wix/2006/wi'})
        product = ET.SubElement(self.root, 'Product', {
            'Name': self.product_name,
            'Manufacturer': self.manufacturer,
            'Id': self.guid,
            'UpgradeCode': self.upgrade_guid,
            'Language': '1033',
            'Codepage':  '1252',
            'Version': self.version,
        })

        package = ET.SubElement(product, 'Package',  {
            'Id': '*',
            'Keywords': 'Installer',
            'Description': '%s %s installer' % (self.name, self.version),
            'Comments': self.comments,
            'Manufacturer': self.manufacturer,
            'InstallerVersion': '500',
            'Languages': '1033',
            'Compressed': 'yes',
            'SummaryCodepage': '1252',
        })

        if self.major_upgrade is not None:
            majorupgrade = ET.SubElement(product, 'MajorUpgrade', {})
            for mkey in self.major_upgrade.keys():
                majorupgrade.set(mkey, self.major_upgrade[mkey])
        else:
            ET.SubElement(product, 'MajorUpgrade', {'DowngradeErrorMessage': 'A newer version of %s is already installed.' % self.name})
        if self.arch == 64:
            package.set('Platform', 'x64')
        ET.SubElement(product, 'Media', {
            'Id': '1',
            'Cabinet': self.basename + '.cab',
            'EmbedCab': 'yes',
        })
        targetdir = ET.SubElement(product, 'Directory', {
            'Id': 'TARGETDIR',
            'Name': 'SourceDir',
        })
        progfiledir = ET.SubElement(targetdir, 'Directory', {
            'Id': self.progfile_dir,
        })
        pmf = ET.SubElement(targetdir, 'Directory', {'Id': 'ProgramMenuFolder'},)
        if self.startmenu_shortcut is not None:
            ET.SubElement(pmf, 'Directory', {
                'Id': 'ApplicationProgramsFolder',
                'Name': self.product_name,
            })
        if self.desktop_shortcut is not None:
            ET.SubElement(pmf, 'Directory', {'Id': 'DesktopFolder',
                                             'Name': 'Desktop',
            })
        installdir = ET.SubElement(progfiledir, 'Directory', {
            'Id': 'INSTALLDIR',
            'Name': self.installdir,
        })
        if self.need_msvcrt:
            ET.SubElement(installdir, 'Merge', {
                'Id': 'VCRedist',
                'SourceFile': self.redist_path,
                'DiskId': '1',
                'Language': '0',
            })

        if self.startmenu_shortcut is not None:
            ap = ET.SubElement(product, 'DirectoryRef', {'Id': 'ApplicationProgramsFolder'})
            comp = ET.SubElement(ap, 'Component', {'Id': 'ApplicationShortcut',
                                                   'Guid': gen_guid(),
                                                   })
            ET.SubElement(comp, 'Shortcut', {'Id': 'ApplicationStartMenuShortcut',
                                             'Name': self.product_name,
                                             'Description': self.comments,
                                             'Target': '[INSTALLDIR]' + self.startmenu_shortcut,
                                             'WorkingDirectory': 'INSTALLDIR',
            })
            ET.SubElement(comp, 'RemoveFolder', {'Id': 'RemoveApplicationProgramsFolder',
                                                 'Directory': 'ApplicationProgramsFolder',
                                                 'On': 'uninstall',
                                                 })
            ET.SubElement(comp, 'RegistryValue', {'Root': 'HKCU',
                                                  'Key': 'Software\\Microsoft\\' + self.name,
                                                  'Name': 'Installed',
                                                  'Type': 'integer',
                                                  'Value': '1',
                                                  'KeyPath': 'yes',
                                                  })
        if self.desktop_shortcut is not None:
            desk = ET.SubElement(product, 'DirectoryRef', {'Id': 'DesktopFolder'})
            comp = ET.SubElement(desk, 'Component', {'Id':'ApplicationShortcutDesktop',
                                                     'Guid': gen_guid(),
                                                     })
            ET.SubElement(comp, 'Shortcut', {'Id': 'ApplicationDesktopShortcut',
                                             'Name': self.product_name,
                                             'Description': self.comments,
                                             'Target': '[INSTALLDIR]' + self.desktop_shortcut,
                                             'WorkingDirectory': 'INSTALLDIR',
            })
            ET.SubElement(comp, 'RemoveFolder', {'Id': 'RemoveDesktopFolder',
                                                 'Directory': 'DesktopFolder',
                                                 'On': 'uninstall',
                                                 })
            ET.SubElement(comp, 'RegistryValue', {'Root': 'HKCU',
                                                  'Key': 'Software\\Microsoft\\' + self.name,
                                                  'Name': 'Installed',
                                                  'Type': 'integer',
                                                  'Value': '1',
                                                  'KeyPath': 'yes',
                                                  })

        ET.SubElement(product, 'UIRef', {'Id': 'FOnlineInstallDirUI'})
        self.create_previous_install_detection(product)
        self.create_install_directory_ui()

        top_feature = ET.SubElement(product, 'Feature', {
            'Id': 'Complete',
            'Title': self.name + ' ' + self.version,
            'Description': 'The complete package',
            'Display': 'expand',
            'Level': '1',
            'ConfigurableDirectory': 'INSTALLDIR',
        })

        for f in self.parts:
            self.scan_feature(top_feature, installdir, 1, f)

        if self.need_msvcrt:
            vcredist_feature = ET.SubElement(top_feature, 'Feature', {
                'Id': 'VCRedist',
                'Title': 'Visual C++ runtime',
                'AllowAdvertise': 'no',
                'Display': 'hidden',
                'Level': '1',
            })
            ET.SubElement(vcredist_feature, 'MergeRef', {'Id': 'VCRedist'})
        if self.startmenu_shortcut is not None:
            ET.SubElement(top_feature, 'ComponentRef', {'Id': 'ApplicationShortcut'})
        if self.desktop_shortcut is not None:
            ET.SubElement(top_feature, 'ComponentRef', {'Id': 'ApplicationShortcutDesktop'})
        if self.addremove_icon is not None:
            icoid = 'addremoveicon.ico'
            ET.SubElement(product, 'Icon', {'Id': icoid,
                                            'SourceFile': self.addremove_icon,
            })
            ET.SubElement(product, 'Property', {'Id': 'ARPPRODUCTICON',
                                                'Value': icoid,
            })

        if self.registry_entries is not None:
            registry_entries_directory = ET.SubElement(product, 'DirectoryRef', {'Id': 'TARGETDIR'})
            registry_entries_component = ET.SubElement(registry_entries_directory, 'Component', {'Id': 'RegistryEntries', 'Guid': gen_guid()})
            if self.arch == 64:
                registry_entries_component.set('Win64', 'yes')
            ET.SubElement(top_feature, 'ComponentRef', {'Id': 'RegistryEntries'})
            for r in self.registry_entries:
                self.create_registry_entries(registry_entries_component, r)

        ET.ElementTree(self.root).write(self.main_xml, encoding='utf-8', xml_declaration=True)
        # ElementTree can not do prettyprinting so do it manually
        import xml.dom.minidom
        doc = xml.dom.minidom.parse(self.main_xml)
        with open(self.main_xml, 'w') as of:
            of.write(doc.toprettyxml(indent=' '))

    def create_previous_install_detection(self, product: ET.Element) -> None:
        if self.install_location_registry is None:
            return

        search = self.install_location_registry
        prop = ET.SubElement(product, 'Property', {'Id': 'PREVIOUSINSTALLDIR'})
        ET.SubElement(prop, 'RegistrySearch', {
            'Id': 'PreviousInstallDirRegistrySearch',
            'Root': search['root'],
            'Key': search['key'],
            'Name': search['name'],
            'Type': 'directory',
            'Win64': search.get('win64', 'no'),
        })
        ET.SubElement(product, 'CustomAction', {
            'Id': 'SetInstallDirFromPreviousInstall',
            'Property': 'INSTALLDIR',
            'Value': '[PREVIOUSINSTALLDIR]',
        })
        install_ui_sequence = ET.SubElement(product, 'InstallUISequence')
        action = ET.SubElement(install_ui_sequence, 'Custom', {
            'Action': 'SetInstallDirFromPreviousInstall',
            'After': 'AppSearch',
        })
        action.text = 'NOT Installed AND NOT INSTALLDIR AND PREVIOUSINSTALLDIR'

    def create_install_directory_ui(self) -> None:
        fragment = ET.SubElement(self.root, 'Fragment')
        ui = ET.SubElement(fragment, 'UI', {'Id': 'FOnlineInstallDirUI'})
        ET.SubElement(ui, 'TextStyle', {'Id': 'WixUI_Font_Normal', 'FaceName': 'Tahoma', 'Size': '8'})
        ET.SubElement(ui, 'TextStyle', {'Id': 'WixUI_Font_Bigger', 'FaceName': 'Tahoma', 'Size': '12'})
        ET.SubElement(ui, 'TextStyle', {'Id': 'WixUI_Font_Title', 'FaceName': 'Tahoma', 'Size': '9', 'Bold': 'yes'})
        ET.SubElement(ui, 'Property', {'Id': 'DefaultUIFont', 'Value': 'WixUI_Font_Normal'})
        ET.SubElement(ui, 'Property', {'Id': 'ARPNOMODIFY', 'Value': '1'})

        install_dialog = ET.SubElement(ui, 'Dialog', {
            'Id': 'FOnlineInstallDirDlg',
            'Width': '370',
            'Height': '270',
            'Title': '[ProductName] Setup',
        })
        ET.SubElement(install_dialog, 'Control', {
            'Id': 'Title', 'Type': 'Text', 'X': '20', 'Y': '18', 'Width': '330', 'Height': '20',
            'Transparent': 'yes', 'NoPrefix': 'yes', 'Text': '{\\WixUI_Font_Title}Choose installation folder',
        })
        ET.SubElement(install_dialog, 'Control', {
            'Id': 'Description', 'Type': 'Text', 'X': '20', 'Y': '50', 'Width': '330', 'Height': '30',
            'NoPrefix': 'yes', 'Text': 'Install [ProductName] in this folder:',
        })
        ET.SubElement(install_dialog, 'Control', {
            'Id': 'Folder', 'Type': 'PathEdit', 'X': '20', 'Y': '90', 'Width': '330', 'Height': '18',
            'Property': 'INSTALLDIR',
        })
        browse = ET.SubElement(install_dialog, 'Control', {
            'Id': 'ChangeFolder', 'Type': 'PushButton', 'X': '20', 'Y': '118', 'Width': '80', 'Height': '18',
            'Text': 'Browse...',
        })
        publish = ET.SubElement(browse, 'Publish', {'Event': 'SpawnDialog', 'Value': 'FOnlineBrowseDlg'})
        publish.text = '1'
        ET.SubElement(install_dialog, 'Control', {
            'Id': 'BottomLine', 'Type': 'Line', 'X': '0', 'Y': '234', 'Width': '370', 'Height': '0',
        })
        install = ET.SubElement(install_dialog, 'Control', {
            'Id': 'Install', 'Type': 'PushButton', 'X': '232', 'Y': '243', 'Width': '64', 'Height': '17',
            'Default': 'yes', 'Text': 'Install',
        })
        publish = ET.SubElement(install, 'Publish', {'Event': 'SetTargetPath', 'Value': 'INSTALLDIR', 'Order': '1'})
        publish.text = '1'
        publish = ET.SubElement(install, 'Publish', {'Event': 'EndDialog', 'Value': 'Return', 'Order': '2'})
        publish.text = '1'
        cancel = ET.SubElement(install_dialog, 'Control', {
            'Id': 'Cancel', 'Type': 'PushButton', 'X': '304', 'Y': '243', 'Width': '56', 'Height': '17',
            'Cancel': 'yes', 'Text': 'Cancel',
        })
        publish = ET.SubElement(cancel, 'Publish', {'Event': 'SpawnDialog', 'Value': 'CancelDlg'})
        publish.text = '1'

        browse_dialog = ET.SubElement(ui, 'Dialog', {
            'Id': 'FOnlineBrowseDlg',
            'Width': '370',
            'Height': '270',
            'Title': 'Browse for Folder',
        })
        ET.SubElement(browse_dialog, 'Control', {
            'Id': 'Title', 'Type': 'Text', 'X': '20', 'Y': '15', 'Width': '330', 'Height': '20',
            'Transparent': 'yes', 'NoPrefix': 'yes', 'Text': '{\\WixUI_Font_Title}Choose a folder',
        })
        directory_combo = ET.SubElement(browse_dialog, 'Control', {
            'Id': 'DirectoryCombo', 'Type': 'DirectoryCombo', 'X': '20', 'Y': '48', 'Width': '240', 'Height': '80',
            'Property': 'INSTALLDIR', 'Fixed': 'yes',
        })
        ET.SubElement(directory_combo, 'Subscribe', {'Event': 'IgnoreChange', 'Attribute': 'IgnoreChange'})
        up = ET.SubElement(browse_dialog, 'Control', {
            'Id': 'Up', 'Type': 'PushButton', 'X': '270', 'Y': '48', 'Width': '75', 'Height': '18', 'Text': 'Up',
        })
        publish = ET.SubElement(up, 'Publish', {'Event': 'DirectoryListUp', 'Value': '0'})
        publish.text = '1'
        new_folder = ET.SubElement(browse_dialog, 'Control', {
            'Id': 'NewFolder', 'Type': 'PushButton', 'X': '270', 'Y': '74', 'Width': '75', 'Height': '18', 'Text': 'New folder',
        })
        publish = ET.SubElement(new_folder, 'Publish', {'Event': 'DirectoryListNew', 'Value': '0'})
        publish.text = '1'
        ET.SubElement(browse_dialog, 'Control', {
            'Id': 'DirectoryList', 'Type': 'DirectoryList', 'X': '20', 'Y': '102', 'Width': '325', 'Height': '92',
            'Property': 'INSTALLDIR', 'Sunken': 'yes', 'TabSkip': 'no',
        })
        ET.SubElement(browse_dialog, 'Control', {
            'Id': 'PathLabel', 'Type': 'Text', 'X': '20', 'Y': '200', 'Width': '325', 'Height': '10',
            'TabSkip': 'no', 'Text': 'Folder:',
        })
        ET.SubElement(browse_dialog, 'Control', {
            'Id': 'PathEdit', 'Type': 'PathEdit', 'X': '20', 'Y': '212', 'Width': '325', 'Height': '18',
            'Property': 'INSTALLDIR',
        })
        ET.SubElement(browse_dialog, 'Control', {
            'Id': 'BottomLine', 'Type': 'Line', 'X': '0', 'Y': '234', 'Width': '370', 'Height': '0',
        })
        ok = ET.SubElement(browse_dialog, 'Control', {
            'Id': 'OK', 'Type': 'PushButton', 'X': '240', 'Y': '243', 'Width': '56', 'Height': '17',
            'Default': 'yes', 'Text': 'OK',
        })
        publish = ET.SubElement(ok, 'Publish', {'Event': 'SetTargetPath', 'Value': 'INSTALLDIR', 'Order': '1'})
        publish.text = '1'
        publish = ET.SubElement(ok, 'Publish', {'Event': 'EndDialog', 'Value': 'Return', 'Order': '2'})
        publish.text = '1'
        browse_cancel = ET.SubElement(browse_dialog, 'Control', {
            'Id': 'Cancel', 'Type': 'PushButton', 'X': '304', 'Y': '243', 'Width': '56', 'Height': '17',
            'Cancel': 'yes', 'Text': 'Cancel',
        })
        publish = ET.SubElement(browse_cancel, 'Publish', {'Event': 'Reset', 'Value': '0', 'Order': '1'})
        publish.text = '1'
        publish = ET.SubElement(browse_cancel, 'Publish', {'Event': 'EndDialog', 'Value': 'Return', 'Order': '2'})
        publish.text = '1'

        for dialog_id in ('CancelDlg', 'ErrorDlg', 'ExitDialog', 'FatalError', 'FilesInUse', 'MsiRMFilesInUse', 'ProgressDlg', 'UserExit'):
            ET.SubElement(ui, 'DialogRef', {'Id': dialog_id})
        publish = ET.SubElement(ui, 'Publish', {
            'Dialog': 'ExitDialog', 'Control': 'Finish', 'Event': 'EndDialog', 'Value': 'Return', 'Order': '999',
        })
        publish.text = '1'
        install_sequence = ET.SubElement(ui, 'InstallUISequence')
        show = ET.SubElement(install_sequence, 'Show', {'Dialog': 'FOnlineInstallDirDlg', 'Before': 'ProgressDlg'})
        show.text = 'NOT Installed'
        ET.SubElement(fragment, 'UIRef', {'Id': 'WixUI_Common'})

    def create_registry_entries(self, comp: ET.Element, reg: dict[str, str]) -> None:
        reg_key_attrs = {
            'Root': reg['root'],
            'Key': reg['key'],
        }
        action = reg.get('action')
        registry_key = (reg['root'], reg['key'])
        if action == 'createAndRemoveOnUninstall' and registry_key not in self.registry_action_keys:
            # wixl 0.103 does not expose WiX's replacement attributes for the deprecated Action
            # field. Registry values still uninstall correctly there; WiX can additionally remove
            # the empty key without emitting its Action deprecation warning.
            if platform.system() == "Windows":
                reg_key_attrs['ForceCreateOnInstall'] = 'yes'
                reg_key_attrs['ForceDeleteOnUninstall'] = 'yes'
        elif action is not None:
            if action != 'createAndRemoveOnUninstall':
                reg_key_attrs['Action'] = action
        self.registry_action_keys.add(registry_key)
        reg_key = ET.SubElement(comp, 'RegistryKey', reg_key_attrs)
        value_attrs = {
            'Type': reg['type'],
            'Value': reg['value'],
            'KeyPath': reg['key_path'],
        }
        if reg['name'] != '':
            value_attrs['Name'] = reg['name']
        ET.SubElement(reg_key, 'RegistryValue', value_attrs)

    def scan_feature(self, top_feature: ET.Element, installdir: ET.Element, depth: int, feature: dict[str, Any]) -> None:
        _ = depth
        for sd in [feature['staged_dir']]:
            if '/' in sd or '\\' in sd:
                sys.exit('Staged_dir %s must not have a path segment.' % sd)
            nodes: dict[str, Node] = {}
            for root, dirs, files in os.walk(sd):
                cur_node = Node(dirs, files)
                nodes[root] = cur_node
            fdict = {
                'Id': feature['id'],
                'Title': feature['title'],
                'Description': feature['description'],
                'Level': '1'
            }
            if feature.get('absent', 'ab') == 'disallow':
                fdict['Absent'] = 'disallow'
            self.feature_properties[sd] = fdict

            self.feature_components[sd] = []
            self.create_xml(nodes, sd, installdir, sd)
            self.build_features(nodes, top_feature, sd)

    def build_features(self, nodes: dict[str, Node], top_feature: ET.Element, staging_dir: str) -> None:
        _ = nodes
        feature = ET.SubElement(top_feature, 'Feature',  self.feature_properties[staging_dir])
        for component_id in self.feature_components[staging_dir]:
            ET.SubElement(feature, 'ComponentRef', {
                'Id': component_id,
            })

    def path_to_id(self, pathname: str) -> str:
        return pathname.replace('\\', '_').replace('/', '_').replace('#', '_').replace('-', '_')

    def create_xml(self, nodes: dict[str, Node], current_dir: str, parent_xml_node: ET.Element, staging_dir: str) -> None:
        cur_node = nodes[current_dir]
        if cur_node.files:
            component_id = 'ApplicationFiles%d' % self.component_num
            comp_xml_node = ET.SubElement(parent_xml_node, 'Component', {
                'Id': component_id,
                'Guid': gen_guid(),
            })
            self.feature_components[staging_dir].append(component_id)
            if self.arch == 64:
                comp_xml_node.set('Win64', 'yes')
            if platform.system() == "Windows" and self.component_num == 0:
                ET.SubElement(comp_xml_node, 'Environment', {
                    'Id': 'Environment',
                    'Name': 'PATH',
                    'Part': 'last',
                    'System': 'yes',
                    'Action': 'set',
                    'Value': '[INSTALLDIR]',
                })
            self.component_num += 1
            for f in cur_node.files:
                file_id = self.path_to_id(os.path.join(current_dir, f))
                ET.SubElement(comp_xml_node, 'File', {
                    'Id': file_id,
                    'Name': f,
                    'Source': os.path.join(current_dir, f),
                })

        for dirname in cur_node.dirs:
            dir_id = self.path_to_id(os.path.join(current_dir, dirname))
            dir_node = ET.SubElement(parent_xml_node, 'Directory', {
                'Id': dir_id,
                'Name': dirname,
            })
            self.create_xml(nodes, os.path.join(current_dir, dirname), dir_node, staging_dir)

    def build_package(self, wixdir: str = '') -> None:
        """
        wixdir = 'c:\\Program Files\\Wix Toolset v3.11\\bin'
        if platform.system() != "Windows":
            wixdir = '/usr/bin'
        if not os.path.isdir(wixdir):
            wixdir = 'c:\\Program Files (x86)\\Wix Toolset v3.11\\bin'
        if not os.path.isdir(wixdir):
            print("ERROR: This script requires WIX")
            sys.exit(1)
        """
        if platform.system() == "Windows":
            subprocess.check_output([os.path.join(wixdir, 'candle')] + self.args1 + [self.main_xml])
            subprocess.check_output([os.path.join(wixdir, 'light'),
                                   '-ext', 'WixUIExtension',
                                   '-cultures:en-us',
                                   '-dWixUILicenseRtf=' + self.license_file] + \
                                   self.args2 + ['-out', self.final_output, self.main_o])
        else:
            subprocess.check_output([os.path.join(wixdir, 'wixl'), '--ext', 'ui', '-o', self.final_output, self.main_xml])


def run(args: list[str]) -> None:
    if len(args) != 1:
        sys.exit('createmsi.py <msi definition json>')
    jsonfile = args[0]
    if '/' in jsonfile or '\\' in jsonfile:
        sys.exit('Input file %s must not contain a path segment.' % jsonfile)
    p = PackageGenerator(jsonfile)
    p.generate_files()
    p.build_package()


def main() -> None:
    run(sys.argv[1:])

if __name__ == '__main__':
    main()
