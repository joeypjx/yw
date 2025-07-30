Name:           agent
Version:        1.0
Release:        1%{?dist}
Summary:        A C++ agent for system monitoring
License:        GPLv2
URL:            https://example.com
Source0:        agent-1.0.tar.gz

%global debug_package %{nil}

# Build dependencies
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  systemd

# Runtime dependencies
Requires:       systemd

%description
This package contains a C++ agent for system monitoring. It provides an
executable and a systemd service to run it in the background.

%prep
# Unpack the source tarball
%setup -q -n agent-%{version}

%build
# Compile the source code
# Add build-id to the binary
export LDFLAGS="-Wl,--build-id"
make %{?_smp_mflags}

%install
rm -rf %{buildroot}

# Install binary
install -d -m 0755 %{buildroot}/usr/local/bin
install -m 0755 agent-1.0 %{buildroot}/usr/local/bin/agent

# Install configuration directory
install -d -m 0755 %{buildroot}/home/uos/
cp -r config %{buildroot}/home/uos/config

# Install systemd service file
# A basic service file is created here. If you have agent.service in your source,
# you can replace the 'cat' block with:
# install -m 0644 agent.service %{buildroot}%{_unitdir}/agent.service
install -d -m 0755 %{buildroot}%{_unitdir}
cat << EOF > %{buildroot}%{_unitdir}/agent.service
[Unit]
Description=Agent Service
After=network.target

[Service]
Type=simple
WorkingDirectory=/home/uos/
ExecStart=/usr/local/bin/agent
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

%post
# Enable and start the service after installation
%systemd_post agent.service

%preun
# Stop and disable the service before uninstallation
%systemd_preun agent.service

%postun
# Reload systemd daemon after uninstallation
%systemd_postun_with_reload agent.service

%files
%defattr(-,root,root,-)
/usr/local/bin/agent
/home/uos/config
%{_unitdir}/agent.service

%changelog
* Wed Jul 17 2024 Gemini - 1.0-1
- Initial RPM release 
