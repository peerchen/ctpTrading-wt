CREATE TABLE `kline` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `index` int(11) NOT NULL DEFAULT '0',
  `open_time` datetime NOT NULL,
  `open_msec` int(11) NOT NULL DEFAULT 0,
  `close_time` datetime NOT NULL,
  `close_msec` int(11) NOT NULL DEFAULT 0,
  `open_price` decimal(10,0) NOT NULL DEFAULT '0',
  `close_price` decimal(10,0) NOT NULL DEFAULT '0',
  `max_price` decimal(10,0) NOT NULL DEFAULT '0',
  `min_price` decimal(10,0) NOT NULL DEFAULT '0',
  `volume` int(11) NOT NULL DEFAULT '0',
  `type` int(11) NOT NULL DEFAULT 0,
  `mtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB CHARSET=utf8;


CREATE TABLE `tick` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `time` datetime NOT NULL COMMENT '服务端返回时间',
  `msec` int(11) NOT NULL DEFAULT '0',
  `price` decimal(10,2) NOT NULL DEFAULT '0',
  `bid_price1` decimal(10,2) NOT NULL DEFAULT '0',
  `ask_price1` decimal(10,2) NOT NULL DEFAULT '0',
  `volume` int(11) NOT NULL DEFAULT '0',
  `mtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB CHARSET=utf8;


CREATE TABLE `order` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `k_index` int(11) NOT NULL DEFAULT 0,
  `front_id` int(11) NOT NULL DEFAULT 0,
  `session_id` int(11) NOT NULL DEFAULT 0,
  `price` decimal(10,2) NOT NULL DEFAULT '0',
  `is_buy` int(1) NOT NULL DEFAULT 0,
  `is_open` int(11) NOT NULL DEFAULT '0',
  `start_time` datetime NOT NULL COMMENT '发出交易指令时间',
  `start_usec` int(11) NOT NULL DEFAULT 0,
  `first_time` datetime NOT NULL COMMENT '首次得到交易回馈时间',
  `first_usec` int(11) NOT NULL DEFAULT 0,
  `end_time` datetime NOT NULL COMMENT '首次得到交易回馈时间',
  `end_usec` int(11) NOT NULL DEFAULT 0,
  `status` int(11) NOT NULL DEFAULT 0,
  `mtime` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_index_front_session` (`k_index`, `front_id`, `session_id`)
) ENGINE=InnoDB CHARSET=utf8;