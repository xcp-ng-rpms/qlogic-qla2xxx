%define vendor_name Qlogic
%define vendor_label qlogic
%define driver_name qla2xxx

%if %undefined module_dir
%define module_dir updates
%endif

## kernel_version will be set during build because then kernel-devel
## package installs an RPM macro which sets it. This check keeps
## rpmlint happy.
%if %undefined kernel_version
%define kernel_version dummy
%endif

Summary: %{vendor_name} %{driver_name} device drivers
Name: %{vendor_label}-%{driver_name}
Version: 10.01.00.54.80.0_k
Release: 1%{?dist}
License: GPL

Source0: https://code.citrite.net/rest/archive/latest/projects/XS/repos/driver-qlogic-qla2xxx/archive?at=10.01.00.54.80.0_k&format=tgz&prefix=driver-qlogic-qla2xxx-10.01.00.54.80.0_k#/qlogic-qla2xxx-10.01.00.54.80.0_k.tar.gz


Provides: gitsha(https://code.citrite.net/rest/archive/latest/projects/XS/repos/driver-qlogic-qla2xxx/archive?at=10.01.00.54.80.0_k&format=tgz&prefix=driver-qlogic-qla2xxx-10.01.00.54.80.0_k#/qlogic-qla2xxx-10.01.00.54.80.0_k.tar.gz) = e15e6e9671e2d223f354d251b3c32262e1fdb756


BuildRequires: gcc
BuildRequires: kernel-devel
Provides: vendor-driver
Requires: %{name}-firmware
Requires: kernel-uname-r = %{kernel_version}
Requires(post): /usr/sbin/depmod
Requires(postun): /usr/sbin/depmod

%description
%{vendor_name} %{driver_name} device drivers for the Linux Kernel
version %{kernel_version}.

%prep
%autosetup -p1 -n driver-%{name}-%{version}

%build
%{?cov_wrap} %{make_build} -C /lib/modules/%{kernel_version}/build M=$(pwd) KSRC=/lib/modules/%{kernel_version}/build modules

%install
%{?cov_wrap} %{__make} %{?_smp_mflags} -C /lib/modules/%{kernel_version}/build M=$(pwd) INSTALL_MOD_PATH=%{buildroot} INSTALL_MOD_DIR=%{module_dir} DEPMOD=/bin/true modules_install

# mark modules executable so that strip-to-file can strip them
find %{buildroot}/lib/modules/%{kernel_version} -name "*.ko" -type f | xargs chmod u+x

%post
/sbin/depmod %{kernel_version}
%{regenerate_initrd_post}

%postun
/sbin/depmod %{kernel_version}
%{regenerate_initrd_postun}

%posttrans
%{regenerate_initrd_posttrans}

%files
/lib/modules/%{kernel_version}/*/*.ko

%changelog
* Wed Oct 30 2019 Deli Zhang <deli.zhang@citrix.com> - 10.01.00.54.80.0_k-1
- CP-32369: (QL-705,QL-717) Updating qlogic-qla2xxx to 10.01.00.54.80.0-k

* Wed Sep 20 2017 Simon Rowe <simon.rowe@citrix.com> - 8.07.00.56.71-1
- UPD-136: (QL-646) Updating qlogic-qla2xxx to 8.07.00.56.71

