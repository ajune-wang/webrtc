/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.CALLS_REAL_METHODS;
import static org.mockito.Mockito.mock;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.test.InstrumentationRegistry;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.NetworkChangeDetector.ConnectionType;
import org.webrtc.NetworkChangeDetector.NetworkInformation;
import org.webrtc.NetworkMonitorAutoDetect.ConnectivityManagerDelegate;
import org.webrtc.NetworkMonitorAutoDetect.NetworkState;
import org.webrtc.NetworkMonitorAutoDetect.SimpleNetworkCallback;

/**
 * Tests for org.webrtc.EnterpriseCellularNetworkMonitor.
 *
 */
@SuppressLint("NewApi")
@RunWith(BaseJUnit4ClassRunner.class)
public class EnterpriseCellularNetworkMonitorTest {
  class FakeDetector implements NetworkChangeDetector {
    ConnectionType connectionType;
    boolean supportNetworkCallback_;
    List<NetworkInformation> activeNetworkList = new ArrayList<>();
    boolean destroyCalled;
    NetworkChangeDetector.Observer observer;

    @Override
    public ConnectionType getCurrentConnectionType() {
      return connectionType;
    }

    @Override
    public boolean supportNetworkCallback() {
      return supportNetworkCallback_;
    }

    @Override
    @Nullable
    public List<NetworkInformation> getActiveNetworkList() {
      return activeNetworkList;
    }

    @Override
    public void destroy() {
      destroyCalled = true;
    }
  };

  class TestObserver implements NetworkChangeDetector.Observer {
    List<NetworkInformation> onNetworkConnectList = new ArrayList<>();
    List<Long> onNetworkDisconnectList = new ArrayList<>();

    void reset() {
      onNetworkConnectList.clear();
      onNetworkDisconnectList.clear();
    }

    @Override
    public void onConnectionTypeChanged(ConnectionType newConnectionType) {}

    @Override
    public void onNetworkConnect(NetworkInformation networkInfo) {
      onNetworkConnectList.add(networkInfo);
    }

    @Override
    public void onNetworkDisconnect(long networkHandle) {
      onNetworkDisconnectList.add(networkHandle);
    }

    @Override
    public void onNetworkPreference(List<ConnectionType> types, @NetworkPreference int preference) {
    }
  };

  EnterpriseCellularNetworkMonitor Create(
      FakeDetector fakeDetector, NetworkChangeDetector.Observer observer) {
    Context context = InstrumentationRegistry.getTargetContext();
    return new EnterpriseCellularNetworkMonitor(new NetworkChangeDetectorFactory() {
      @Override
      public NetworkChangeDetector create(
          NetworkChangeDetector.Observer observer, Context context) {
        fakeDetector.observer = observer;
        return fakeDetector;
      }
    }, observer, context);
  }

  NetworkChangeDetector.NetworkInformation createNetworkInfo(
      long handle, ConnectionType connectionType, boolean isEnterprise) {
    return new NetworkChangeDetector.NetworkInformation("if:" + connectionType, connectionType,
        NetworkChangeDetector.ConnectionType.CONNECTION_NONE, handle,
        new NetworkChangeDetector.IPAddress[0], isEnterprise);
  }

  @Test
  @UiThreadTest
  @SmallTest
  public void testActiveEnterpriseNetworks() throws InterruptedException {
    FakeDetector fakeDetector = new FakeDetector();
    TestObserver observer = new TestObserver();
    EnterpriseCellularNetworkMonitor monitor = Create(fakeDetector, observer);

    boolean isEnterprise = true;
    final NetworkChangeDetector.NetworkInformation net1 =
        createNetworkInfo(1, NetworkChangeDetector.ConnectionType.CONNECTION_WIFI, isEnterprise);
    final NetworkChangeDetector.NetworkInformation net2 =
        createNetworkInfo(2, NetworkChangeDetector.ConnectionType.CONNECTION_4G, isEnterprise);

    fakeDetector.activeNetworkList.add(net1);
    fakeDetector.activeNetworkList.add(net2);

    ArrayList<NetworkChangeDetector.NetworkInformation> expectList = new ArrayList<>();
    expectList.add(net1);
    expectList.add(net2);
    assertEquals(expectList, monitor.getActiveNetworkList());
  }

  @Test
  @UiThreadTest
  @SmallTest
  public void testActiveMixedNetworks() throws InterruptedException {
    FakeDetector fakeDetector = new FakeDetector();
    TestObserver observer = new TestObserver();
    EnterpriseCellularNetworkMonitor monitor = Create(fakeDetector, observer);

    boolean isEnterprise = true;
    boolean notEnterprise = false;
    final NetworkChangeDetector.NetworkInformation net1 =
        createNetworkInfo(1, NetworkChangeDetector.ConnectionType.CONNECTION_WIFI, isEnterprise);
    final NetworkChangeDetector.NetworkInformation net2 =
        createNetworkInfo(2, NetworkChangeDetector.ConnectionType.CONNECTION_4G, isEnterprise);
    final NetworkChangeDetector.NetworkInformation net3 =
        createNetworkInfo(3, NetworkChangeDetector.ConnectionType.CONNECTION_4G, notEnterprise);

    fakeDetector.activeNetworkList.add(net1);
    fakeDetector.activeNetworkList.add(net2);
    fakeDetector.activeNetworkList.add(net3);

    ArrayList<NetworkChangeDetector.NetworkInformation> expectList = new ArrayList<>();
    expectList.add(net1);
    expectList.add(net2);
    assertEquals(expectList, monitor.getActiveNetworkList());
  }

  @Test
  @UiThreadTest
  @SmallTest
  public void testActiveNonEnterpriseNetworks() throws InterruptedException {
    FakeDetector fakeDetector = new FakeDetector();
    TestObserver observer = new TestObserver();
    EnterpriseCellularNetworkMonitor monitor = Create(fakeDetector, observer);

    boolean isEnterprise = true;
    boolean notEnterprise = false;
    final NetworkChangeDetector.NetworkInformation net1 =
        createNetworkInfo(1, NetworkChangeDetector.ConnectionType.CONNECTION_WIFI, isEnterprise);
    final NetworkChangeDetector.NetworkInformation net2 =
        createNetworkInfo(2, NetworkChangeDetector.ConnectionType.CONNECTION_4G, notEnterprise);
    final NetworkChangeDetector.NetworkInformation net3 =
        createNetworkInfo(3, NetworkChangeDetector.ConnectionType.CONNECTION_4G, notEnterprise);

    fakeDetector.activeNetworkList.add(net1);
    fakeDetector.activeNetworkList.add(net2);
    fakeDetector.activeNetworkList.add(net3);

    ArrayList<NetworkChangeDetector.NetworkInformation> expectList = new ArrayList<>();
    expectList.add(net1);
    expectList.add(net2);
    expectList.add(net3);
    assertEquals(expectList, monitor.getActiveNetworkList());
  }
}
