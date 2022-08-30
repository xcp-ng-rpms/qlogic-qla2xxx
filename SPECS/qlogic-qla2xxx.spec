%global package_speccommit dd37d4ca3ed1dff48f132d6a5fe480d2a0e9f721
%global usver 10.01.00.54.80.0_k
%global xsver 3
%global xsrel %{xsver}%{?xscount}%{?xshash}
%global package_srccommit 10.01.00.54.80.0_k
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
Release: %{?xsrel}%{?dist}
License: GPL
Source0: qlogic-qla2xxx-10.01.00.54.80.0_k.tar.gz

BuildRequires: gcc
BuildRequires: kernel-devel
%{?_cov_buildrequires}
Provides: vendor-driver
Requires: %{name}-firmware
Requires: kernel-uname-r = %{kernel_version}
Requires(post): /usr/sbin/depmod
Requires(postun): /usr/sbin/depmod

%description
%{vendor_name} %{driver_name} device drivers for the Linux Kernel
version %{kernel_version}.

%prep
%autosetup -p1 -n %{name}-%{version}
%{?_cov_prepare}

%build
%{?_cov_wrap} %{make_build} -C /lib/modules/%{kernel_version}/build M=$(pwd) KSRC=/lib/modules/%{kernel_version}/build modules

%install
%{?_cov_wrap} %{__make} %{?_smp_mflags} -C /lib/modules/%{kernel_version}/build M=$(pwd) INSTALL_MOD_PATH=%{buildroot} INSTALL_MOD_DIR=%{module_dir} DEPMOD=/bin/true modules_install

# mark modules executable so that strip-to-file can strip them
find %{buildroot}/lib/modules/%{kernel_version} -name "*.ko" -type f | xargs chmod u+x

%{?_cov_install}

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

%{?_cov_results_package}

%changelog
* Mon Feb 14 2022 Ross Lagerwall <ross.lagerwall@citrix.com> - 10.01.00.54.80.0_k-3
- CP-38416: Enable static analysis

* Wed Dec 02 2020 Ross Lagerwall <ross.lagerwall@citrix.com> - 10.01.00.54.80.0_k-2
- CP-35517: Fix the build for koji

* Wed Oct 30 2019 Deli Zhang <deli.zhang@citrix.com> - 10.01.00.54.80.0_k-1
- CP-32369: (QL-705,QL-717) Updating qlogic-qla2xxx to 10.01.00.54.80.0-k

* Wed Sep 20 2017 Simon Rowe <simon.rowe@citrix.com> - 8.07.00.56.71-1
- UPD-136: (QL-646) Updating qlogic-qla2xxx to 8.07.00.56.71
