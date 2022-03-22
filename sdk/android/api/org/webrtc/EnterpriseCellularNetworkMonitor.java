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

import android.content.Context;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * NetworkChangeDetector that filters cellular networks wrt to NET_CAPABILITY_ENTERPRISE. If any
 * cellular network is enterprise, only enterprise cellular networks will be returned.
 */
public class EnterpriseCellularNetworkMonitor implements NetworkChangeDetector {
  private final NetworkChangeDetector monitor;
  private final NetworkChangeDetector.Observer observer;
  private final HashMap<Long, NetworkInformation> enterpriseNetworks = new HashMap<>();
  private final HashMap<Long, NetworkInformation> nonEnterpriseNetworks = new HashMap<>();

  EnterpriseCellularNetworkMonitor(NetworkChangeDetectorFactory factory,
      NetworkChangeDetector.Observer observer, Context context) {
    this.observer = observer;
    this.monitor = factory.create(new NetworkChangeDetector.Observer() {
      @Override
      public void onConnectionTypeChanged(ConnectionType newConnectionType) {
        observer.onConnectionTypeChanged(newConnectionType);
      }

      @Override
      public void onNetworkConnect(NetworkInformation networkInfo) {
        onNetworkConnectImpl(networkInfo);
      }

      @Override
      public void onNetworkDisconnect(long networkHandle) {
        onNetworkDisconnectImpl(networkHandle);
      }

      @Override
      public void onNetworkPreference(
          List<ConnectionType> types, @NetworkPreference int preference) {
        observer.onNetworkPreference(types, preference);
      }
    }, context);
  }

  @Override
  public ConnectionType getCurrentConnectionType() {
    return monitor.getCurrentConnectionType();
  }

  @Override
  public boolean supportNetworkCallback() {
    return monitor.supportNetworkCallback();
  }

  @Override
  @Nullable
  public List<NetworkInformation> getActiveNetworkList() {
    List<NetworkInformation> networkList = monitor.getActiveNetworkList();
    System.out.println("KESO 1: getActiveNetworkList() " + toString(networkList));
    List<NetworkInformation> returnList = new ArrayList<>();
    for (NetworkInformation networkInfo : networkList) {
      if (!isCellular(networkInfo)) {
        returnList.add(networkInfo);
      } else {
        addNetwork(networkInfo);
      }
    }
    returnList.addAll(getNetworks().values());
    System.out.println("KESO 2: getActiveNetworkList() => return: " + toString(returnList));
    return returnList;
  }

  @Override
  public void destroy() {
    monitor.destroy();
  }

  private void onNetworkConnectImpl(NetworkInformation networkInfo) {
    System.out.println("KESO 3: onNetworkConnect: " + networkInfo);
    if (!isCellular(networkInfo)) {
      System.out.println("KESO 4: observer.onNetworkConnect: " + networkInfo);
      observer.onNetworkConnect(networkInfo);
      return;
    }

    HashMap<Long, NetworkInformation> before =
        (HashMap<Long, NetworkInformation>) getNetworks().clone();
    addNetwork(networkInfo);
    HashMap<Long, NetworkInformation> after = getNetworks();

    for (NetworkInformation info : after.values()) {
      if (!before.containsKey(info.handle)) {
        System.out.println("KESO 5: observer.onNetworkConnect: " + info);
        observer.onNetworkConnect(info);
      }
    }

    for (NetworkInformation info : before.values()) {
      if (!after.containsKey(info.handle)) {
        System.out.println("KESO 6: observer.onNetworkDisconnect: " + info);
        observer.onNetworkDisconnect(info.handle);
      }
    }

    if (after.containsKey(networkInfo.handle) && before.containsKey(networkInfo.handle)) {
      System.out.println("KESO 7: observer.onNetworkConnect: " + networkInfo);
      observer.onNetworkConnect(networkInfo);
    }
  }

  private void onNetworkDisconnectImpl(long networkHandle) {
    System.out.println("KESO 8: onNetworkDisconnect: " + networkHandle);
    if (!isCellular(networkHandle)) {
      System.out.println("KESO 9: observer.onNetworkDisconnect: " + networkHandle);
      observer.onNetworkDisconnect(networkHandle);
      return;
    }

    HashMap<Long, NetworkInformation> before =
        (HashMap<Long, NetworkInformation>) getNetworks().clone();
    removeNetwork(networkHandle);
    HashMap<Long, NetworkInformation> after = getNetworks();

    for (NetworkInformation info : after.values()) {
      if (!before.containsKey(networkHandle)) {
        System.out.println("KESO 10: observer.onNetworkConnect: " + info);
        observer.onNetworkConnect(info);
      }
    }

    for (NetworkInformation info : before.values()) {
      if (!after.containsKey(networkHandle)) {
        System.out.println("KESO 11: observer.onNetworkDisconnect: " + info);
        observer.onNetworkDisconnect(info.handle);
      }
    }
  }

  private String toString(List<NetworkInformation> networkInfoList) {
    StringBuilder s = new StringBuilder();
    s.append("[ ");
    for (NetworkInformation networkInfo : networkInfoList) {
      s.append(networkInfo.toString());
      s.append(" ");
    }
    s.append("]");
    return s.toString();
  }

  private void addNetwork(NetworkInformation networkInfo) {
    if (networkInfo.isEnterprise) {
      enterpriseNetworks.put(networkInfo.handle, networkInfo);
      nonEnterpriseNetworks.remove(networkInfo.handle);
    } else {
      enterpriseNetworks.remove(networkInfo.handle);
      nonEnterpriseNetworks.put(networkInfo.handle, networkInfo);
    }
  }

  private void removeNetwork(long networkHandle) {
    enterpriseNetworks.remove(networkHandle);
    nonEnterpriseNetworks.remove(networkHandle);
  }

  private HashMap<Long, NetworkInformation> getNetworks() {
    if (!enterpriseNetworks.isEmpty()) {
      return enterpriseNetworks;
    }
    return nonEnterpriseNetworks;
  }

  private boolean isCellular(long networkHandle) {
    if (enterpriseNetworks.containsKey(networkHandle)) {
      return true;
    } else if (nonEnterpriseNetworks.containsKey(networkHandle)) {
      return true;
    }
    return false;
  }

  private static boolean isCellular(NetworkInformation networkInfo) {
    NetworkChangeDetector.ConnectionType type = networkInfo.type;
    if (type == NetworkChangeDetector.ConnectionType.CONNECTION_VPN) {
      type = networkInfo.underlyingTypeForVpn;
    }
    switch (type) {
      case CONNECTION_5G:
      case CONNECTION_4G:
      case CONNECTION_3G:
      case CONNECTION_2G:
      case CONNECTION_UNKNOWN_CELLULAR:
        return true;
      default:
        return false;
    }
  }
}
