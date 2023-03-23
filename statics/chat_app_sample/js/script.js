var chat_screen = null;
var chat_input = null;
var send_button = null;
var ws = null;
var authorid = null;
var cookie_info = {};
var user_dict = {};

const request_history = () => {
  if (ws === null || authorid === null) {
    return;
  }
  console.log("send", "request history");
  ws.send(
    JSON.stringify({
      cmd: "request",
      args: ["history"],
    })
  );
};

const request_userinfo = () => {
  if (ws === null || authorid === null) {
    return;
  }
  console.log("send", "request userinfo");
  ws.send(
    JSON.stringify({
      cmd: "request",
      args: ["userinfo"],
    })
  );
};

const update_chat_screen = (history) => {
  if (chat_screen === null || chat_screen === undefined) {
    return;
  }
  Array.from(chat_screen.childNodes).forEach((log) => {
    chat_screen.removeChild(log);
  });

  history = Array.from(history);
  history.sort((a, b) => (a.time > b.time ? 1 : -1));
  history.forEach((info) => {
    add_chat_log(info.message, info.authorid, info.time);
  });
};

const pad = (num, length=2) => {
  let text = Array(length).fill("0").join("") + num.toString();
  console.log(text);
  return text.substring(text.length - length, text.length);
}

const add_chat_log = (__message, __authorid, __time) => {
  if (chat_screen === null || chat_screen === undefined) {
    return;
  }
  let user_name = user_dict[__authorid] || "Unknown";
  __time = Number(__time);
  let d = new Date();
  d.setTime(__time);
  console.log(d);
  let isSelf = __authorid === authorid;
  let chat_panel_area = document.createElement("div");
  chat_panel_area.classList.add("chat_panel_area");
  let blank = document.createElement("div");
  blank.classList.add("blank");
  let chat_info = document.createElement("div");
  chat_info.classList.add("chat_info");
  let chat_author = document.createElement("div");
  chat_author.classList.add("chat_author");
  chat_author.innerText = user_name;
  let post_time = document.createElement("div");
  post_time.classList.add("post_time");
  post_time.innerText = pad(d.getHours()) + ":" + pad(d.getMinutes());
  let chat_panel = document.createElement("div");
  chat_panel.innerText = __message;
  chat_panel.classList.add(isSelf ? "chat_panel_self" : "chat_panel");
  chat_info.appendChild(chat_author);
  chat_info.appendChild(post_time);
  if (isSelf) {
    chat_panel_area.appendChild(blank);
    chat_panel_area.appendChild(chat_info);
    chat_panel_area.appendChild(chat_panel);
  } else {
    chat_panel_area.appendChild(chat_info);
    chat_panel_area.appendChild(chat_panel);
    chat_panel_area.appendChild(blank);
  }
  chat_screen.appendChild(chat_panel_area);
  setTimeout(() => {
    chat_screen.scrollTop = chat_screen.scrollHeight;
  });
};

const connectWs = () => {
  ws = new WebSocket(`ws://${window.location.host}`);
  ws.onopen = (event) => {
    console.log("onopen", event);
    if (authorid === null) {
      // userid cannot be found from cookie => request userid
      window.location.pathname = "/index.html";
    } else {
      console.log("send", "request set_userid");
      ws.send(
        JSON.stringify({
          cmd: "request",
          args: ["set_userid", authorid],
        })
      );
      // else => restore uuid from cookie
      // set uuid as author id
      // authorid = cookie["uuid"];
    }

    // request history to backend
  };

  ws.onmessage = (event) => {
    let info = JSON.parse(event.data);
    console.log("onmessage", info);
    if (info.subject === undefined || info.subject === null) {
      return;
    } else if (info.subject === "Error") {
      delete_cookie("authorid");
      window.location.pathname = "/index.html";
    } else if (info.subject === "userid") {
      authorid = info.authorid;
      set_cookie("authorid", authorid, 7);
    } else if (info.subject === "set_userid") {
      request_userinfo();
    } else if (info.subject === "history") {
      update_chat_screen(info.body);
    } else if (info.subject === "userinfo") {
      // set_userinfo
      body = Array.from(info.body);
      body.forEach((user) => {
        user_dict[user.id] = user.name;
      });
      request_history();
    } else if (info.subject === "onMessage") {
      add_chat_log(info.message, info.authorid, info.time);
    }
  };

  ws.onclose = (event) => {
    console.log("onclose", event.data);
    setTimeout(() => {
      connectWs();
    }, 1000);
    // show error message?
  };
};

window.onfocus = () => {
  console.log("onfocus");
  if (chat_input === undefined) {
    return;
  }

  chat_input.focus(); 
}

window.onload = () => {
  chat_screen = document.getElementById("chat_screen");
  chat_input = document.getElementById("chat_input");
  send_button = document.getElementById("send_button");

  if(chat_input === undefined)   {
    return;
  }

  chat_input.focus()  

  authorid = get_cookie("authorid");

  connectWs();
  console.log("onload");
  console.log(chat_screen);
  console.log(chat_input.value);
  console.log(send_button);

  send_button.onclick = (event) => {
    send_chat_message();
  };

  chat_input.onkeydown = (event) => {
    if (event.key === "Enter") {
      send_chat_message();
    }
  };

  const send_chat_message = () => {
    if (ws === null || chat_input === null) {
      return;
    }
    if (chat_input.value === null || chat_input.value.trim() === "") {
      return;
    }
    // send message
    console.log("send", "msg", chat_input.value);
    // clear form value
    let unixtime = new Date().getTime();
    ws.send(
      JSON.stringify({
        cmd: "msg",
        args: [chat_input.value],
        time: unixtime,
      })
    );
    chat_input.value = "";
  };
};
